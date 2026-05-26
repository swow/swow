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
use PHPUnit\Framework\TestCase;
use Swow\Process\WorkerContext;

/**
 * @internal
 */
#[CoversClass(WorkerContext::class)]
final class WorkerContextTest extends TestCase
{
    public function testGetters(): void
    {
        $ctx = new WorkerContext(
            id: 2,
            pid: 12345,
            managerPid: 12000,
            workerCount: 4,
        );

        $this->assertSame(2, $ctx->getId());
        $this->assertSame(12345, $ctx->getPid());
        $this->assertSame(12000, $ctx->getManagerPid());
        $this->assertSame(4, $ctx->getWorkerCount());
    }

    public function testZeroBasedId(): void
    {
        $ctx = new WorkerContext(0, 100, 99, 8);
        $this->assertSame(0, $ctx->getId());
    }
}
