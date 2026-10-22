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

use function sprintf;

use const PHP_OS_FAMILY;
use const STDERR;

/**
 * Shared process manager lifecycle.
 *
 * The manager starts workers and controls their life cycle.
 * It restarts workers that exit with an error.
 * On POSIX, SIGUSR1 starts a graceful reload. On Windows, SIGBREAK starts it.
 * SIGTERM and SIGINT start a graceful stop.
 */
trait ProcessManagerTrait
{
    /** @var int Number of workers */
    protected int $workerCount;

    /** @var int Maximum restart count for one worker after an error */
    protected int $maxRestartCount = 100;

    /** @var int Maximum graceful stop wait time in seconds */
    protected int $maxWaitTime = 30;

    /** @var array<int, ChildProcess> Worker ID to process */
    protected array $workers = [];

    /** @var array<int, int> Worker ID to restart count */
    protected array $restartCounts = [];

    /** @var bool Manager running state */
    protected bool $running = false;

    /** @var bool Reload state */
    protected bool $reloading = false;

    /** @var Closure(int, int, ProcessExitStatus): void|null Worker exit callback */
    protected ?Closure $onWorkerExit = null;

    /** @var bool Manager coroutine loop state */
    protected bool $managingCoroutines = false;

    /** @var int Number of active coroutines that the manager owns */
    protected int $managedCoroutineCount = 0;

    /** @var Coroutine|null SIGTERM listener coroutine */
    protected ?Coroutine $termCoroutine = null;

    /** @var Coroutine|null SIGINT listener coroutine */
    protected ?Coroutine $intCoroutine = null;

    /** @var Coroutine|null Reload signal listener coroutine */
    protected ?Coroutine $reloadCoroutine = null;

    /** @var Coroutine|null Active reload coroutine */
    protected ?Coroutine $reloadTaskCoroutine = null;

    /** @var array<int, Coroutine> Worker ID to monitor coroutine */
    protected array $watcherCoroutines = [];

    /**
     * @param int $workerCount Number of workers. 0 selects the CPU count.
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
     * Maximum number of restarts after an error.
     * The manager does not restart the worker after this limit.
     */
    public function setMaxRestartCount(int $count): static
    {
        $this->maxRestartCount = $count;
        return $this;
    }

    /**
     * Maximum time in seconds to wait for a graceful stop.
     * The manager uses SIGKILL after this time.
     */
    public function setMaxWaitTime(int $seconds): static
    {
        $this->maxWaitTime = $seconds;
        return $this;
    }

    /**
     * Set the worker exit callback.
     *
     * The callback runs in the manager process.
     * It receives the worker ID, PID, and exit status.
     *
     * @param Closure(int $workerId, int $workerPid, ProcessExitStatus $status): void $callback
     */
    public function onWorkerExit(Closure $callback): static
    {
        $this->onWorkerExit = $callback;
        return $this;
    }

    /**
     * Stop all workers gracefully.
     */
    public function stop(): void
    {
        $this->running = false;
        $managedByManager = $this->managingCoroutines;

        foreach ($this->workers as $process) {
            if (!$process->hasExited()) {
                $this->killWorker($process, Signal::TERM);
            }
        }

        if (!$managedByManager) {
            /* Wait for all workers to exit. */
            foreach ($this->workers as $id => $process) {
                if (!$process->hasExited()) {
                    try {
                        $process->wait($this->maxWaitTime * 1000);
                    } catch (ProcessException) {
                        /* The wait timed out. Kill the worker. */
                        $this->killWorker($process, Signal::KILL);
                        try {
                            $process->wait(1000);
                        } catch (ProcessException) {
                            /* ignore */
                        }
                    }
                }
            }

            $this->workers = [];
        }

        $current = Coroutine::getCurrent();
        if (!$managedByManager) {
            /* Kill the remaining signal coroutines. Do not kill the current coroutine. */
            foreach ([$this->termCoroutine, $this->intCoroutine, $this->reloadCoroutine, $this->reloadTaskCoroutine] as $coroutine) {
                if ($coroutine !== null && $coroutine !== $current && $coroutine->isAvailable()) {
                    $coroutine->kill();
                }
            }
            $this->termCoroutine = null;
            $this->intCoroutine = null;
            $this->reloadCoroutine = null;
            $this->reloadTaskCoroutine = null;
        }

        if (!$managedByManager) {
            /* Watcher coroutines exit after their workers. Kill them if they remain. */
            foreach ($this->watcherCoroutines as $coroutine) {
                if ($coroutine !== $current && $coroutine->isAvailable()) {
                    $coroutine->kill();
                }
            }
            $this->watcherCoroutines = [];
        }
    }

    protected function runManagedCoroutine(Closure $callback): Coroutine
    {
        $this->managedCoroutineCount++;

        return Coroutine::run(function () use ($callback): void {
            try {
                $callback();
            } finally {
                $this->managedCoroutineCount--;
            }
        });
    }

    protected function enforceWorkerShutdownTimeout(): void
    {
        $deadline = microtime(true) + $this->maxWaitTime;
        while ($this->workers !== [] && microtime(true) < $deadline) {
            usleep(50000);
        }

        foreach ($this->workers as $process) {
            if (!$process->hasExited()) {
                $this->killWorker($process, Signal::KILL);
            }
        }
    }

    /**
     * Reload all workers one by one without downtime.
     */
    public function reload(): void
    {
        if ($this->reloading) {
            return;
        }
        $this->reloading = true;

        /* Copy the current worker list. */
        $oldWorkers = $this->workers;

        foreach ($oldWorkers as $id => $oldProcess) {
            if (!$this->running) {
                break;
            }

            /* Start the new worker first. The service stays available. */
            $this->createWorker($id);
            $newProcess = $this->workers[$id] ?? null;
            if ($newProcess !== null) {
                $this->watchWorker($id, $newProcess);
            }

            /* Ask the old worker to exit. */
            if (!$oldProcess->hasExited()) {
                $this->killWorker($oldProcess, Signal::TERM);
                try {
                    $oldProcess->wait($this->maxWaitTime * 1000);
                } catch (ProcessException) {
                    $this->killWorker($oldProcess, Signal::KILL);
                    try {
                        $oldProcess->wait(1000);
                    } catch (ProcessException) {
                        /* ignore */
                    }
                }
            }

            /* Reset the restart count for this worker. */
            $this->restartCounts[$id] = 0;
        }

        $this->reloading = false;
    }

    private function killWorker(ChildProcess $process, int $signal): void
    {
        try {
            $process->kill($signal);
        } catch (ProcessException) {
            /* The worker may exit between hasExited() and kill(). */
        }
    }

    abstract protected function createWorker(int $id): ChildProcess;

    protected function startManager(): void
    {
        $this->running = true;

        for ($i = 0; $i < $this->workerCount; $i++) {
            $this->workers[$i] = $this->createWorker($i);
        }

        $this->handleSignals();
    }

    /**
     * Run the manager loop.
     *
     * The manager handles signals and collects workers.
     * Each worker has a wait coroutine. The coroutine restarts the worker when necessary.
     * The manager waits for the coroutines that it owns. After stop, the signal coroutines exit by timeout.
     */
    protected function handleSignals(): void
    {
        $this->managingCoroutines = true;
        $this->managedCoroutineCount = 0;

        /* Start a wait coroutine for each worker. */
        foreach ($this->workers as $id => $process) {
            $this->watchWorker($id);
        }

        /* SIGTERM starts a graceful stop. */
        $this->termCoroutine = $this->runManagedCoroutine(function (): void {
            while ($this->running) {
                try {
                    Signal::wait(Signal::TERM, self::SIGNAL_POLL_TIMEOUT);
                } catch (\Swow\SignalException) {
                    continue;
                }
                $this->stop();
                return;
            }
        });

        /* SIGINT starts a graceful stop. */
        $this->intCoroutine = $this->runManagedCoroutine(function (): void {
            while ($this->running) {
                try {
                    Signal::wait(Signal::INT, self::SIGNAL_POLL_TIMEOUT);
                } catch (\Swow\SignalException) {
                    continue;
                }
                $this->stop();
                return;
            }
        });

        /* SIGUSR1 starts a graceful reload. Windows uses SIGBREAK. */
        $reloadSignal = PHP_OS_FAMILY === 'Windows' ? Signal::BREAK : Signal::USR1;
        $this->reloadCoroutine = $this->runManagedCoroutine(function () use ($reloadSignal): void {
            while ($this->running) {
                try {
                    Signal::wait($reloadSignal, self::SIGNAL_POLL_TIMEOUT);
                } catch (\Swow\SignalException) {
                    continue;
                }
                if ($this->reloadTaskCoroutine === null || !$this->reloadTaskCoroutine->isAvailable()) {
                    $this->reloadTaskCoroutine = $this->runManagedCoroutine(function (): void {
                        $this->reload();
                    });
                }
            }
        });

        $this->runManagedCoroutine(function (): void {
            while ($this->running) {
                usleep(50000);
            }
            $this->enforceWorkerShutdownTimeout();
        });

        try {
            /* The manager waits for the coroutines that it owns. */
            while ($this->managedCoroutineCount > 0) {
                usleep(10000);
            }
        } finally {
            $this->managingCoroutines = false;
            $this->managedCoroutineCount = 0;
            $this->termCoroutine = null;
            $this->intCoroutine = null;
            $this->reloadCoroutine = null;
            $this->reloadTaskCoroutine = null;
            $this->watcherCoroutines = [];
        }
    }

    /**
     * Start a monitor coroutine for a worker.
     * The coroutine restarts the worker according to the restart policy.
     */
    protected function watchWorker(int $id, ?ChildProcess $process = null): void
    {
        $process ??= $this->workers[$id] ?? null;
        if ($process === null) {
            return;
        }

        $this->watcherCoroutines[$id] = $this->runManagedCoroutine(function () use ($id, $process): void {
            $status = null;
            try {
                $status = $process->wait();
            } catch (ProcessException) {
                /* The wait failed. Another caller collected the process. */
                return;
            }

            if (($this->workers[$id] ?? null) !== $process) {
                return;
            }

            if ($this->onWorkerExit !== null) {
                ($this->onWorkerExit)($id, $process->getPid(), $status);
            }

            if (!$this->running) {
                unset($this->workers[$id]);
                return;
            }

            /* Restart the worker if the restart limit allows. */
            if ($this->restartCounts[$id] < $this->maxRestartCount) {
                $this->restartCounts[$id]++;
                $this->createWorker($id);
                $newProcess = $this->workers[$id] ?? null;
                if ($newProcess !== null) {
                    $this->watchWorker($id, $newProcess);
                }
                return;
            }

            fwrite(STDERR, sprintf(
                "[Manager] Worker#%d exceeded max restart count (%d), giving up\n",
                $id,
                $this->maxRestartCount
            ));
            unset($this->workers[$id]);
        });
    }
}
