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
use Swow\Process\ProcessExitStatus;
use Swow\Process\ProcessManager;
use Swow\Process\WorkerContext;
use Swow\Signal;

use function count;

use const FILE_APPEND;
use const FILE_IGNORE_NEW_LINES;
use const FILE_SKIP_EMPTY_LINES;
use const LOCK_EX;

/** @internal */
#[CoversClass(ProcessManager::class)]
#[RequiresOperatingSystem('Linux|Darwin')]
final class ProcessManagerLifecycleTest extends TestCase
{
    public function testExitedWorkersRestartWithFreshProcessIds(): void
    {
        $manager = (new ProcessManager(1))->setMaxRestartCount(1)->setMaxWaitTime(0);
        $exits = [];
        $manager->onWorkerExit(static function (int $id, int $pid, ProcessExitStatus $status) use ($manager, &$exits): void {
            $exits[] = [$id, $pid, $status];
            if (count($exits) === 2) {
                $manager->stop();
            }
        });
        $loop = Coroutine::run(static function () use ($manager): void {
            $manager->start(static function (): void {
                usleep(20000);
            });
        });
        try {
            $this->waitForManagerExit($loop);
            $this->assertCount(2, $exits);
            $this->assertSame([0, 0], array_column($exits, 0));
            $this->assertNotSame($exits[0][1], $exits[1][1]);
            foreach ($exits as [, $pid, $status]) {
                $this->assertGreaterThan(0, $pid);
                $this->assertTrue($status->isExited());
                $this->assertSame(0, $status->getExitCode());
            }
            $this->assertFalse($loop->isAlive());
        } finally {
            $manager->stop();
            if ($loop->isAlive()) {
                $this->waitForManagerExit($loop);
            }
        }
    }

    public function testReloadReplacesWorkersAndReportsOnlyCurrentWorkerExit(): void
    {
        $readyFile = tempnam(sys_get_temp_dir(), 'swow-manager-reload-');
        $this->assertIsString($readyFile);
        $manager = (new ProcessManager(1))->setMaxRestartCount(0)->setMaxWaitTime(1);
        $exits = [];
        $manager->onWorkerExit(static function (int $id, int $pid, ProcessExitStatus $status) use (&$exits): void {
            $exits[] = [$id, $pid, $status];
        });
        $loop = Coroutine::run(static function () use ($manager, $readyFile): void {
            $manager->start(static function (WorkerContext $context) use ($readyFile): void {
                file_put_contents($readyFile, $context->getPid() . "\n", FILE_APPEND | LOCK_EX);
                Signal::wait(Signal::TERM, 5000);
            });
        });
        try {
            $original = $this->waitForWorkerRecords($readyFile, 1);
            $manager->reload();
            $replacement = $this->waitForWorkerRecords($readyFile, 2);
            $this->assertSame($original[0], $replacement[0]);
            $this->assertNotSame($replacement[0], $replacement[1]);
            $this->assertSame([], $exits);
            $manager->stop();
            $this->waitForManagerExit($loop);
            $this->assertCount(1, $exits);
            $this->assertSame(0, $exits[0][0]);
            $this->assertSame((int) $replacement[1], $exits[0][1]);
            $this->assertTrue($exits[0][2]->isExited());
            $this->assertSame(0, $exits[0][2]->getExitCode());
            $this->assertFalse($loop->isAlive());
        } finally {
            $manager->stop();
            if ($loop->isAlive()) {
                $this->waitForManagerExit($loop);
            }
            unlink($readyFile);
        }
    }

    public function testShutdownTimeoutKillsAnUnresponsiveWorker(): void
    {
        $readyFile = tempnam(sys_get_temp_dir(), 'swow-manager-timeout-');
        $this->assertIsString($readyFile);
        $manager = (new ProcessManager(1))->setMaxWaitTime(0);
        $exits = [];
        $manager->onWorkerExit(static function (int $id, int $pid, ProcessExitStatus $status) use (&$exits): void {
            $exits[] = $status;
        });
        $loop = Coroutine::run(static function () use ($manager, $readyFile): void {
            $manager->start(static function (WorkerContext $context) use ($readyFile): void {
                file_put_contents($readyFile, $context->getPid() . "\n");
                Signal::wait(Signal::TERM, 5000);
                // An active worker may delay graceful shutdown beyond the configured deadline.
                while (true) {
                    usleep(50000);
                }
            });
        });
        try {
            $this->waitForWorkerRecords($readyFile, 1);
            $manager->stop();
            $this->waitForManagerExit($loop);
            $this->assertCount(1, $exits);
            $this->assertTrue($exits[0]->isSignaled());
            $this->assertSame(Signal::KILL, $exits[0]->getTermSignal());
            $this->assertFalse($loop->isAlive());
        } finally {
            $manager->stop();
            if ($loop->isAlive()) {
                $this->waitForManagerExit($loop);
            }
            unlink($readyFile);
        }
    }

    private function waitForManagerExit(Coroutine $loop): void
    {
        $deadline = microtime(true) + 5.0;
        while ($loop->isAlive() && microtime(true) < $deadline) {
            usleep(10000);
        }
        $this->assertFalse($loop->isAlive(), 'The manager must reap its workers and return');
    }

    /** @return list<string> */
    private function waitForWorkerRecords(string $file, int $count): array
    {
        $deadline = microtime(true) + 5.0;
        do {
            $records = file($file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
            $this->assertIsArray($records);
            if (count($records) >= $count) {
                return $records;
            }
            usleep(10000);
        } while (microtime(true) < $deadline);

        $this->fail('Workers did not report readiness before the deadline');
    }
}
