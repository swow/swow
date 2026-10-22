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
use Throwable;

use function sprintf;

use const STDERR;

/**
 * POSIX-only process manager backed by ChildProcess::fork().
 */
final class ForkProcessManager implements ProcessManagerInterface
{
    use ProcessManagerTrait;

    private const SIGNAL_POLL_TIMEOUT = 100;

    private ?Closure $onWorkerStart = null;

    public function start(Closure $onWorkerStart): void
    {
        if (PHP_OS_FAMILY === 'Windows') {
            throw new ProcessManagerException('ForkProcessManager is only supported on Unix-like systems');
        }

        $this->onWorkerStart = $onWorkerStart;
        $this->startManager();
    }

    protected function createWorker(int $id): ChildProcess
    {
        $managerPid = getmypid();
        $workerCount = $this->workerCount;
        $onWorkerStart = $this->onWorkerStart;

        $process = ChildProcess::fork(static function () use ($id, $managerPid, $workerCount, $onWorkerStart): int {
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

        return $process;
    }
}
