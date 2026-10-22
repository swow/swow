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

namespace Swow\Tests\Process;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\RequiresOperatingSystem;
use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Process\ChildProcess;
use Swow\Process\ProcessExitStatus;
use Swow\Process\ForkProcessManager;
use Swow\Process\WorkerContext;
use Swow\Signal;
use Throwable;

/**
 * @internal
 */
#[CoversClass(ForkProcessManager::class)]
#[RequiresOperatingSystem('Linux|Darwin')]
final class ForkProcessManagerTest extends TestCase
{
    public function testWorkerCountDefault(): void
    {
        $manager = new ForkProcessManager(4);
        $this->assertSame(4, $manager->getWorkerCount());
    }

    public function testWorkerCountAutoDetect(): void
    {
        $manager = new ForkProcessManager(0);
        $this->assertGreaterThan(0, $manager->getWorkerCount());
        $this->assertSame(\Swow\nproc(), $manager->getWorkerCount());
    }

    public function testSetMaxRestartCountFluent(): void
    {
        $manager = new ForkProcessManager(2);
        $result = $manager->setMaxRestartCount(50);
        $this->assertSame($manager, $result);
    }

    public function testSetMaxWaitTimeFluent(): void
    {
        $manager = new ForkProcessManager(2);
        $result = $manager->setMaxWaitTime(10);
        $this->assertSame($manager, $result);
    }

    public function testOnWorkerExitFluent(): void
    {
        $manager = new ForkProcessManager(1);
        $result = $manager->onWorkerExit(static function (): void {
        });

        $this->assertSame($manager, $result);
    }

    public function testStartAndStop(): void
    {
        $manager = new ForkProcessManager(2);
        $manager->setMaxWaitTime(3);

        /* Start the manager in a coroutine. Another coroutine stops it later. */
        $started = false;
        $workerIds = [];

        Coroutine::run(static function () use ($manager, &$started, &$workerIds): void {
            $manager->start(static function (WorkerContext $ctx) use (&$started, &$workerIds): void {
                $started = true;
                $workerIds[] = $ctx->getId();
                // The worker exits after a short time.
                usleep(200000);
            });
        });

        // Wait for the manager to start and the workers to exit.
        usleep(500000);
        $manager->stop();

        // start() forks the workers. Check the manager API here.
        $this->assertSame(2, $manager->getWorkerCount());
    }

    public function testChildProcessIntegration(): void
    {
        // Test ChildProcess with ProcessManager directly.
        $process = ChildProcess::fork(static function (): int {
            // Simulate simple worker logic.
            usleep(100000);
            return 7;
        });

        $this->assertGreaterThan(0, $process->getPid());
        $this->assertFalse($process->hasExited());

        $status = $process->wait();
        $this->assertTrue($process->hasExited());
        $this->assertSame(7, $status->getExitCode());
    }

    public function testOnWorkerExitReceivesWorkerStatus(): void
    {
        $seen = [];
        $manager = new ForkProcessManager(1);
        $manager->setMaxRestartCount(0);
        $manager->onWorkerExit(static function (int $id, int $pid, ProcessExitStatus $status) use (&$seen): void {
            $seen[] = [$id, $pid, $status->getExitCode()];
        });

        Coroutine::run(static function () use ($manager): void {
            $manager->start(static function (): void {
                usleep(100000);
            });
        });

        usleep(300000);
        $manager->stop();

        $this->assertCount(1, $seen);
        $this->assertSame(0, $seen[0][0]);
        $this->assertGreaterThan(0, $seen[0][1]);
        $this->assertSame(0, $seen[0][2]);
    }

    public function testStopDoesNotWaitForExternalCoroutines(): void
    {
        $businessCoroutineStopped = false;
        $businessCoroutine = Coroutine::run(static function () use (&$businessCoroutineStopped): void {
            try {
                while (true) {
                    usleep(50000);
                }
            } finally {
                $businessCoroutineStopped = true;
            }
        });

        try {
            $manager = new ForkProcessManager(1);
            Coroutine::run(static function () use ($manager): void {
                $manager->start(static function (): void {
                    while (true) {
                        usleep(50000);
                    }
                });
            });

            usleep(200000);
            $manager->stop();

            $this->assertFalse($businessCoroutineStopped);
        } finally {
            if ($businessCoroutine->isAvailable()) {
                $businessCoroutine->kill();
            }
        }
    }

    public function testSignalStopReapsWorkersAndReturns(): void
    {
        $readyFile = tempnam(sys_get_temp_dir(), 'swow-process-manager-ready-');
        $this->assertIsString($readyFile);
        $process = ChildProcess::fork(static function () use ($readyFile): int {
            $manager = new ForkProcessManager(1);
            $manager->setMaxWaitTime(3);
            $manager->start(static function () use ($readyFile): void {
                file_put_contents($readyFile, 'ready');
                while (true) {
                    usleep(50000);
                }
            });

            return 0;
        });

        try {
            $deadline = microtime(true) + 5.0;
            do {
                if (file_get_contents($readyFile) === 'ready') {
                    break;
                }
                usleep(10000);
            } while (microtime(true) < $deadline);

            $this->assertSame('ready', file_get_contents($readyFile));

            $process->kill(Signal::TERM);
            $status = $process->wait(5000);

            $this->assertTrue($status->isExited());
            $this->assertSame(0, $status->getExitCode());
        } finally {
            if (!$process->hasExited()) {
                $process->kill(Signal::KILL);
                try {
                    $process->wait(1000);
                } catch (Throwable) {
                    /* ignore */
                }
            }
            unlink($readyFile);
        }
    }
}
