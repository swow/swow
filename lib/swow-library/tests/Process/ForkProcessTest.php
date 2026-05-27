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
use Swow\Process\ForkProcess;
use Swow\Process\ProcessException;
use Swow\Process\ProcessExitStatus;
use Swow\Process\ProcessInterface;
use Swow\Signal;

/**
 * @internal
 */
#[CoversClass(ForkProcess::class)]
#[RequiresOperatingSystem('Linux|Darwin')]
final class ForkProcessTest extends TestCase
{
    public function testForkReturnsProcess(): void
    {
        $process = ForkProcess::fork(static fn(): int => 0);
        $this->assertInstanceOf(ForkProcess::class, $process);
        $this->assertInstanceOf(ProcessInterface::class, $process);
        $this->assertGreaterThan(0, $process->getPid());
        $process->wait();
    }

    public function testForkCallbackExitCode(): void
    {
        $process = ForkProcess::fork(static fn(): int => 42);
        $status = $process->wait();

        $this->assertInstanceOf(ProcessExitStatus::class, $status);
        $this->assertTrue($status->isExited());
        $this->assertFalse($status->isSignaled());
        $this->assertSame(42, $status->getExitCode());
        $this->assertSame(0, $status->getTermSignal());
    }

    public function testForkCallbackZeroExitCode(): void
    {
        $process = ForkProcess::fork(static fn(): int => 0);
        $status = $process->wait();

        $this->assertTrue($status->isExited());
        $this->assertSame(0, $status->getExitCode());
    }

    public function testHasExitedBeforeAndAfterWait(): void
    {
        $process = ForkProcess::fork(static fn(): int => 0);

        // 可能还没退出（竞态），但 wait 后一定是 true
        $process->wait();
        $this->assertTrue($process->hasExited());
    }

    public function testWaitThrowsOnAlreadyExited(): void
    {
        $process = ForkProcess::fork(static fn(): int => 0);
        $process->wait();

        $this->expectException(ProcessException::class);
        $process->wait();
    }

    public function testKill(): void
    {
        $process = ForkProcess::fork(static function (): int {
            // 子进程持续运行
            while (true) {
                usleep(50000);
            }
            return 0; // unreachable
        });

        usleep(100000); // 等子进程启动
        $process->kill(Signal::KILL);
        $status = $process->wait();

        $this->assertTrue($status->isSignaled());
        $this->assertFalse($status->isExited());
        $this->assertSame(Signal::KILL, $status->getTermSignal());
    }

    public function testKillWithTerm(): void
    {
        $process = ForkProcess::fork(static function (): int {
            while (true) {
                usleep(50000);
            }
            return 0;
        });

        usleep(100000);
        $process->kill(); // 默认 SIGTERM
        $status = $process->wait();

        $this->assertTrue($status->isSignaled());
        $this->assertSame(Signal::TERM, $status->getTermSignal());
    }

    public function testKillThrowsOnAlreadyExited(): void
    {
        $process = ForkProcess::fork(static fn(): int => 0);
        $process->wait();

        $this->expectException(ProcessException::class);
        $process->kill();
    }

    public function testMultipleForks(): void
    {
        $processes = [];
        for ($i = 0; $i < 4; $i++) {
            $processes[$i] = ForkProcess::fork(static fn(): int => $i);
        }

        $this->assertCount(4, $processes);

        $pids = [];
        foreach ($processes as $i => $process) {
            $pids[] = $process->getPid();
            $status = $process->wait();
            $this->assertSame($i, $status->getExitCode());
        }

        // 所有 PID 应不同
        $this->assertCount(4, array_unique($pids));
    }

    public function testChildPidDiffersFromParent(): void
    {
        $parentPid = getmypid();

        $process = ForkProcess::fork(static fn(): int => 0);
        $childPid = $process->getPid();

        $this->assertNotSame($parentPid, $childPid);
        $process->wait();
    }
}
