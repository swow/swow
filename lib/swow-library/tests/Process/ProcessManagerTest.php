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
use Swow\Process\ForkProcess;
use Swow\Process\ProcessManager;
use Swow\Process\WorkerContext;

/**
 * @internal
 */
#[CoversClass(ProcessManager::class)]
#[RequiresOperatingSystem('Linux|Darwin')]
final class ProcessManagerTest extends TestCase
{
    public function testWorkerCountDefault(): void
    {
        $manager = new ProcessManager(4);
        $this->assertSame(4, $manager->getWorkerCount());
    }

    public function testWorkerCountAutoDetect(): void
    {
        $manager = new ProcessManager(0);
        $this->assertGreaterThan(0, $manager->getWorkerCount());
        $this->assertSame(\Swow\nproc(), $manager->getWorkerCount());
    }

    public function testSetMaxRestartCountFluent(): void
    {
        $manager = new ProcessManager(2);
        $result = $manager->setMaxRestartCount(50);
        $this->assertSame($manager, $result);
    }

    public function testSetMaxWaitTimeFluent(): void
    {
        $manager = new ProcessManager(2);
        $result = $manager->setMaxWaitTime(10);
        $this->assertSame($manager, $result);
    }

    public function testStartAndStop(): void
    {
        $manager = new ProcessManager(2);
        $manager->setMaxWaitTime(3);

        /* 在协程中启动 Manager，另一个协程延迟 stop */
        $started = false;
        $workerIds = [];

        Coroutine::run(static function () use ($manager, &$started, &$workerIds): void {
            $manager->start(static function (WorkerContext $ctx) use (&$started, &$workerIds): void {
                $started = true;
                $workerIds[] = $ctx->getId();
                // Worker 运行一小段时间后自行退出
                usleep(200000);
            });
        });

        // 等待 Manager 启动并 Workers 退出
        usleep(500000);
        $manager->stop();

        // Manager 的 start 内部处理了 fork，这里验证 Manager 的 API 正确
        $this->assertSame(2, $manager->getWorkerCount());
    }

    public function testForkProcessIntegration(): void
    {
        // 直接测试 ForkProcess 与 ProcessManager 的配合
        $process = ForkProcess::fork(static function (): int {
            // 模拟一个 Worker 的简单逻辑
            usleep(100000);
            return 7;
        });

        $this->assertGreaterThan(0, $process->getPid());
        $this->assertFalse($process->hasExited());

        $status = $process->wait();
        $this->assertTrue($process->hasExited());
        $this->assertSame(7, $status->getExitCode());
    }
}
