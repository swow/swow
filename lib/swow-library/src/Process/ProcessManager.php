<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Process;

use Closure;
use Swow\Coroutine;
use Swow\Signal;
use Throwable;

use function sprintf;

use const STDERR;

/**
 * 多进程管理器
 *
 * 将当前进程变为 Manager 角色，fork 出 N 个 Worker 进程。
 * 管理 Worker 生命周期：异常退出自动重启、SIGUSR1 优雅重载、SIGTERM 优雅关闭。
 */
class ProcessManager
{
    /** @var int Worker 数量 */
    protected int $workerCount;

    /** @var int 单个 Worker 最大异常重启次数 */
    protected int $maxRestartCount = 100;

    /** @var int Worker 优雅退出最大等待时间（秒） */
    protected int $maxWaitTime = 30;

    /** @var array<int, ForkProcess> Worker ID → ForkProcess */
    protected array $workers = [];

    /** @var array<int, int> Worker ID → 已重启次数 */
    protected array $restartCounts = [];

    /** @var bool Manager 是否正在运行 */
    protected bool $running = false;

    /** @var bool 是否正在进行 reload */
    protected bool $reloading = false;

    /** @var Closure|null Worker 启动回调 */
    protected ?Closure $onWorkerStart = null;

    /** @var Coroutine|null SIGTERM 监听协程 */
    protected ?Coroutine $termCoroutine = null;

    /** @var Coroutine|null SIGINT 监听协程 */
    protected ?Coroutine $intCoroutine = null;

    /** @var Coroutine|null SIGUSR1 监听协程 */
    protected ?Coroutine $reloadCoroutine = null;

    /** @var array<int, Coroutine> Worker 监控协程（Worker ID → Coroutine） */
    protected array $watcherCoroutines = [];

    /**
     * @param int $workerCount Worker 数量，0 = CPU 核数
     */
    public function __construct(int $workerCount = 0)
    {
        if ($workerCount <= 0) {
            $workerCount = \Swow\nproc();
        }
        $this->workerCount = $workerCount;
    }

    public function getWorkerCount(): int
    {
        return $this->workerCount;
    }

    /**
     * 单个 Worker 最大异常重启次数，超过后不再自动拉起
     */
    public function setMaxRestartCount(int $count): static
    {
        $this->maxRestartCount = $count;
        return $this;
    }

    /**
     * Worker 优雅退出最大等待时间（秒），超时后 SIGKILL
     */
    public function setMaxWaitTime(int $seconds): static
    {
        $this->maxWaitTime = $seconds;
        return $this;
    }

    /**
     * 启动多进程管理器
     *
     * 当前进程变为 Manager，阻塞直到所有 Worker 退出。
     *
     * @param Closure(WorkerContext): void $onWorkerStart 每个 Worker 的启动逻辑
     */
    public function start(Closure $onWorkerStart): void
    {
        $this->onWorkerStart = $onWorkerStart;
        $this->running = true;

        /* 启动所有 Worker */
        for ($i = 0; $i < $this->workerCount; $i++) {
            $this->forkWorker($i);
        }

        /* 注册信号处理 */
        $this->handleSignals();
    }

    /**
     * 优雅关闭所有 Worker
     */
    public function stop(): void
    {
        $this->running = false;

        foreach ($this->workers as $process) {
            if (!$process->hasExited()) {
                $process->kill(Signal::TERM);
            }
        }

        /* 等待所有 Worker 退出 */
        foreach ($this->workers as $id => $process) {
            if (!$process->hasExited()) {
                try {
                    $process->wait($this->maxWaitTime * 1000);
                } catch (ProcessException) {
                    /* 超时，强制杀死 */
                    $process->kill(Signal::KILL);
                    try {
                        $process->wait(1000);
                    } catch (ProcessException) {
                        /* ignore */
                    }
                }
            }
        }

        $this->workers = [];

        /* kill 残留的信号协程（排除调用 stop 的当前协程） */
        $current = Coroutine::getCurrent();
        foreach ([$this->termCoroutine, $this->intCoroutine, $this->reloadCoroutine] as $coroutine) {
            if ($coroutine !== null && $coroutine !== $current && $coroutine->isAvailable()) {
                $coroutine->kill();
            }
        }
        $this->termCoroutine = null;
        $this->intCoroutine = null;
        $this->reloadCoroutine = null;

        /* watcherCoroutines 在 worker 退出后会自动结束，但以防万一 */
        foreach ($this->watcherCoroutines as $coroutine) {
            if ($coroutine->isAvailable()) {
                $coroutine->kill();
            }
        }
        $this->watcherCoroutines = [];
    }

    /**
     * 优雅重载：逐个重启 Worker，零停机
     */
    public function reload(): void
    {
        if ($this->reloading) {
            return;
        }
        $this->reloading = true;

        /* 快照当前 Worker 列表 */
        $oldWorkers = $this->workers;

        foreach ($oldWorkers as $id => $oldProcess) {
            if (!$this->running) {
                break;
            }

            /* 先 fork 新 Worker（确保服务不中断） */
            $this->forkWorker($id);

            /* 再通知旧 Worker 退出 */
            if (!$oldProcess->hasExited()) {
                $oldProcess->kill(Signal::TERM);
                try {
                    $oldProcess->wait($this->maxWaitTime * 1000);
                } catch (ProcessException) {
                    $oldProcess->kill(Signal::KILL);
                    try {
                        $oldProcess->wait(1000);
                    } catch (ProcessException) {
                        /* ignore */
                    }
                }
            }

            /* 重置该 Worker 的重启计数 */
            $this->restartCounts[$id] = 0;
        }

        $this->reloading = false;
    }

    /**
     * Fork 一个 Worker 进程
     */
    protected function forkWorker(int $id): void
    {
        $managerPid = getmypid();
        $workerCount = $this->workerCount;
        $onWorkerStart = $this->onWorkerStart;

        $process = ForkProcess::fork(static function () use ($id, $managerPid, $workerCount, $onWorkerStart): int {
            $ctx = new WorkerContext($id, getmypid(), $managerPid, $workerCount);
            try {
                $onWorkerStart($ctx);
                return 0;
            } catch (Throwable $e) {
                fwrite(STDERR, sprintf(
                    "[Worker#%d] Uncaught %s: %s in %s:%d\n",
                    $id,
                    $e::class,
                    $e->getMessage(),
                    $e->getFile(),
                    $e->getLine()
                ));
                return 1;
            }
        });

        $this->workers[$id] = $process;
        if (!isset($this->restartCounts[$id])) {
            $this->restartCounts[$id] = 0;
        }
    }

    /**
     * Manager 主循环：处理信号 + 回收 Worker
     *
     * 为每个 Worker 启动一个等待协程，Worker 退出时自动处理重启逻辑。
     * 主协程使用 waitAll 等待所有协程退出，信号协程在 stop 时被主动 kill。
     */
    protected function handleSignals(): void
    {
        /* 为每个 Worker 启动一个 wait 协程 */
        foreach ($this->workers as $id => $process) {
            $this->watchWorker($id);
        }

        /* SIGTERM → 优雅关闭 */
        $this->termCoroutine = Coroutine::run(function (): void {
            while ($this->running) {
                try {
                    Signal::wait(Signal::TERM);
                } catch (\Swow\SignalException) {
                    return;
                }
                $this->stop();
                return;
            }
        });

        /* SIGINT → 优雅关闭 */
        $this->intCoroutine = Coroutine::run(function (): void {
            while ($this->running) {
                try {
                    Signal::wait(Signal::INT);
                } catch (\Swow\SignalException) {
                    return;
                }
                $this->stop();
                return;
            }
        });

        /* SIGUSR1 → 优雅重载 */
        $this->reloadCoroutine = Coroutine::run(function (): void {
            while ($this->running) {
                try {
                    Signal::wait(Signal::USR1);
                } catch (\Swow\SignalException) {
                    return;
                }
                $this->reload();
            }
        });

        /* Manager 主协程等待直到所有协程退出 */
        \Swow\Sync\waitAll();
    }

    /**
     * 为 Worker 启动一个监控协程
     * Worker 退出时根据策略自动重启
     */
    protected function watchWorker(int $id): void
    {
        $this->watcherCoroutines[$id] = Coroutine::run(function () use ($id): void {
            $process = $this->workers[$id] ?? null;
            if ($process === null) {
                return;
            }

            try {
                $process->wait();
            } catch (ProcessException) {
                /* wait 失败（进程已被其他方式回收） */
            }

            if (!$this->running) {
                unset($this->workers[$id]);
                return;
            }

            /* 异常退出时自动重启 */
            if ($this->restartCounts[$id] < $this->maxRestartCount) {
                $this->restartCounts[$id]++;
                $this->forkWorker($id);
                $this->watchWorker($id);
            } else {
                fwrite(STDERR, sprintf(
                    "[Manager] Worker#%d exceeded max restart count (%d), giving up\n",
                    $id,
                    $this->maxRestartCount
                ));
                unset($this->workers[$id]);
            }
        });
    }
}
