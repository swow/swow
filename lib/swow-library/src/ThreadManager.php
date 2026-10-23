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

namespace Swow;

use Closure;
use ReflectionClass;
use Throwable;

use function dirname;
use function filter_var;
use function ini_get;
use function is_file;
use function serialize;
use function sprintf;
use function usleep;
use function var_export;

use const FILTER_VALIDATE_BOOLEAN;
use const PHP_OS_FAMILY;
use const STDERR;

/**
 * Manages a set of worker threads.
 *
 * The manager starts workers and controls their life cycle.
 * It restarts workers that exit while the manager runs.
 * On POSIX, SIGUSR1 starts a graceful reload. On Windows, SIGBREAK starts it.
 * SIGTERM and SIGINT start a graceful stop.
 */
final class ThreadManager implements ThreadManagerInterface
{
    private const SIGNAL_POLL_TIMEOUT = 100;

    /** @var int Number of workers */
    private int $workerCount;

    /** @var int Maximum restart count for one worker */
    private int $maxRestartCount = 100;

    /** @var int Maximum graceful stop wait time in seconds */
    private int $maxWaitTime = 30;

    /** @var array<int, Thread> Worker ID to thread */
    private array $workers = [];

    /** @var array<int, int> Worker ID to restart count */
    private array $restartCounts = [];

    private bool $running = false;

    private bool $reloading = false;

    /** @var Closure(int, int, int): void|null */
    private ?Closure $onWorkerExit = null;

    private string $mode = '';

    private ?string $inlineCode = null;

    private ?string $closurePayload = null;

    private ?string $autoloadPath = null;

    private ?string $contextFile = null;

    private int $managerThreadId = 0;

    private int $managedCoroutineCount = 0;

    private ?Coroutine $termCoroutine = null;

    private ?Coroutine $intCoroutine = null;

    private ?Coroutine $reloadCoroutine = null;

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

    public function setMaxRestartCount(int $count): static
    {
        $this->maxRestartCount = $count;
        return $this;
    }

    public function setMaxWaitTime(int $seconds): static
    {
        $this->maxWaitTime = $seconds;
        return $this;
    }

    /**
     * @param Closure(int $workerId, int $threadId, int $exitStatus): void $callback
     */
    public function onWorkerExit(Closure $callback): static
    {
        $this->onWorkerExit = $callback;
        return $this;
    }

    public function start(Closure $onWorkerStart): void
    {
        $this->assertClosureSerializerEnabled();

        try {
            $this->closurePayload = serialize($onWorkerStart);
        } catch (Throwable $e) {
            throw new ThreadManagerException('Unable to serialize the worker closure', 0, $e);
        }

        $this->mode = 'closure';
        $this->startManager();
    }

    public function startInline(string $code): void
    {
        if ($code === '') {
            throw new ThreadManagerException('Inline worker code must not be empty');
        }

        $this->inlineCode = $code;
        $this->mode = 'inline';
        $this->startManager();
    }

    public function stop(): void
    {
        $this->running = false;

        foreach ($this->workers as $thread) {
            if ($thread->isRunning()) {
                try {
                    $thread->kill();
                } catch (ThreadException) {
                    /* The thread may exit between isRunning() and kill(). */
                }
            }
        }

        foreach ($this->workers as $thread) {
            try {
                $thread->wait($this->maxWaitTime * 1000);
            } catch (ThreadException) {
                try {
                    $thread->detach();
                } catch (ThreadException) {
                    /* ignore */
                }
            }
        }

        $this->workers = [];
        $this->restartCounts = [];

        $current = Coroutine::getCurrent();
        foreach ([$this->termCoroutine, $this->intCoroutine, $this->reloadCoroutine] as $coroutine) {
            if ($coroutine !== null && $coroutine !== $current && $coroutine->isAvailable()) {
                $coroutine->kill();
            }
        }
        $this->termCoroutine = null;
        $this->intCoroutine = null;
        $this->reloadCoroutine = null;
    }

    public function reload(): void
    {
        if ($this->reloading) {
            return;
        }
        $this->reloading = true;

        $oldWorkers = $this->workers;

        foreach ($oldWorkers as $id => $oldThread) {
            if (!$this->running) {
                break;
            }

            $newThread = $this->createWorker($id);
            $this->watchWorker($id, $newThread);

            try {
                $oldThread->wait($this->maxWaitTime * 1000);
            } catch (ThreadException) {
                try {
                    $oldThread->kill();
                    $oldThread->wait(1000);
                } catch (ThreadException) {
                    try {
                        $oldThread->detach();
                    } catch (ThreadException) {
                        /* ignore */
                    }
                }
            }

            $this->restartCounts[$id] = 0;
        }

        $this->reloading = false;
    }

    private function startManager(): void
    {
        if ($this->running) {
            throw new ThreadManagerException('ThreadManager has already been started');
        }

        $this->running = true;
        $this->managerThreadId = Thread::getCurrentId();
        $this->autoloadPath = $this->findAutoloadPath();
        $this->contextFile = __DIR__ . '/ThreadWorkerContext.php';

        for ($i = 0; $i < $this->workerCount; $i++) {
            $this->workers[$i] = $this->createWorker($i);
            $this->watchWorker($i, $this->workers[$i]);
        }

        $this->handleSignals();
    }

    private function createWorker(int $id): Thread
    {
        $thread = match ($this->mode) {
            'closure' => $this->createClosureWorker($id),
            'inline' => $this->createInlineWorker($id),
            default => throw new ThreadManagerException('ThreadManager has not been started'),
        };

        $this->workers[$id] = $thread;
        $this->restartCounts[$id] ??= 0;

        return $thread;
    }

    private function createClosureWorker(int $id): Thread
    {
        return Thread::runCode($this->buildClosureCode($id));
    }

    private function createInlineWorker(int $id): Thread
    {
        return Thread::runCode($this->buildInlineCode($id));
    }

    private function buildClosureCode(int $id): string
    {
        if ($this->closurePayload === null) {
            throw new ThreadManagerException('Closure worker callback is missing');
        }

        return $this->buildBootstrapCode($id, [
            '$__swow_thread_manager_callback = unserialize(' .
            var_export($this->closurePayload, true) . ', [\'allowed_classes\' => true]);',
            'if (!$__swow_thread_manager_callback instanceof \Closure) {',
            '    throw new \RuntimeException(\'Worker closure payload is invalid\');',
            '}',
            '$__swow_thread_manager_callback($context);',
        ]);
    }

    private function buildInlineCode(int $id): string
    {
        return $this->buildBootstrapCode($id, [
            'eval(' . var_export($this->inlineCode, true) . ');',
        ]);
    }

    /** @param list<string> $body */
    private function buildBootstrapCode(int $id, array $body): string
    {
        $code = [
            '$__swow_thread_manager_autoload = ' . var_export($this->autoloadPath, true) . ';',
            '$__swow_thread_manager_context_file = ' . var_export($this->contextFile, true) . ';',
            'if ($__swow_thread_manager_autoload !== null) {',
            '    require $__swow_thread_manager_autoload;',
            '} else {',
            '    require $__swow_thread_manager_context_file;',
            '}',
            sprintf(
                '$context = new \Swow\ThreadWorkerContext(%d, \Swow\Thread::getCurrentId(), %d, %d);',
                $id,
                $this->managerThreadId,
                $this->workerCount,
            ),
            ...$body,
        ];

        return implode("\n", $code);
    }

    private function watchWorker(int $id, Thread $thread): void
    {
        $this->runManagedCoroutine(function () use ($id, $thread): void {
            $status = 0;

            try {
                $thread->wait();
                $status = $thread->getExitStatus();
            } catch (Throwable) {
                $status = $thread->getExitStatus();
            }

            if (($this->workers[$id] ?? null) !== $thread) {
                return;
            }

            unset($this->workers[$id]);

            if ($this->onWorkerExit !== null) {
                ($this->onWorkerExit)($id, $thread->getId(), $status);
            }

            if (!$this->running) {
                return;
            }

            if (($this->restartCounts[$id] ?? 0) < $this->maxRestartCount) {
                $this->restartCounts[$id]++;
                $newThread = $this->createWorker($id);
                $this->watchWorker($id, $newThread);
                return;
            }

            if ($this->maxRestartCount > 0) {
                fwrite(STDERR, sprintf(
                    "[ThreadManager] Worker#%d exceeded max restart count (%d), giving up\n",
                    $id,
                    $this->maxRestartCount,
                ));
            }
        });
    }

    private function handleSignals(): void
    {
        $this->termCoroutine = $this->runManagedCoroutine(function (): void {
            while ($this->running) {
                try {
                    Signal::wait(Signal::TERM, self::SIGNAL_POLL_TIMEOUT);
                } catch (SignalException) {
                    continue;
                }
                $this->stop();
                return;
            }
        });

        $this->intCoroutine = $this->runManagedCoroutine(function (): void {
            while ($this->running) {
                try {
                    Signal::wait(Signal::INT, self::SIGNAL_POLL_TIMEOUT);
                } catch (SignalException) {
                    continue;
                }
                $this->stop();
                return;
            }
        });

        $reloadSignal = PHP_OS_FAMILY === 'Windows' ? Signal::BREAK : Signal::USR1;
        $this->reloadCoroutine = $this->runManagedCoroutine(function () use ($reloadSignal): void {
            while ($this->running) {
                try {
                    Signal::wait($reloadSignal, self::SIGNAL_POLL_TIMEOUT);
                } catch (SignalException) {
                    continue;
                }
                $this->reload();
            }
        });

        $this->runManagedCoroutine(function (): void {
            while ($this->running) {
                usleep(50000);
            }
            $this->enforceShutdownTimeout();
        });

        try {
            while ($this->managedCoroutineCount > 0) {
                usleep(10000);
            }
        } finally {
            $this->managedCoroutineCount = 0;
            $this->termCoroutine = null;
            $this->intCoroutine = null;
            $this->reloadCoroutine = null;
        }
    }

    private function enforceShutdownTimeout(): void
    {
        $deadline = microtime(true) + $this->maxWaitTime;
        while ($this->workers !== [] && microtime(true) < $deadline) {
            usleep(50000);
        }

        foreach ($this->workers as $thread) {
            try {
                $thread->kill();
            } catch (ThreadException) {
                /* ignore */
            }
        }
    }

    private function runManagedCoroutine(Closure $callback): Coroutine
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

    private function assertClosureSerializerEnabled(): void
    {
        if (!filter_var(ini_get('swow.closure_serializer'), FILTER_VALIDATE_BOOLEAN)) {
            throw new ThreadManagerException('swow.closure_serializer is not enabled');
        }
    }

    private function findAutoloadPath(): ?string
    {
        if (!class_exists(\Composer\Autoload\ClassLoader::class)) {
            return null;
        }

        $reflection = new ReflectionClass(\Composer\Autoload\ClassLoader::class);
        $file = $reflection->getFileName();
        if ($file === false) {
            return null;
        }

        $path = dirname($file, 2) . '/autoload.php';

        return is_file($path) ? $path : null;
    }
}
