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

namespace Swow\Tests\Debug\Debugger;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use stdClass;
use Swow\Debug\Debugger\DebuggerException;
use Swow\Debug\Debugger\DebuggerHelper;

/** @internal */
#[CoversClass(DebuggerHelper::class)]
final class DebuggerHelperTest extends TestCase
{
    public function testFrameFormatting(): void
    {
        $this->assertSame('Unknown', DebuggerHelper::convertFrameToExecutingString([]));
        $this->assertSame('run()', DebuggerHelper::convertFrameToExecutingString(['function' => 'run']));
        $this->assertSame(
            "Worker::handle(42, 1.5, null, true, false, 'abcdefgh...', [], [...], stdClass{})",
            DebuggerHelper::convertFrameToExecutingString([
                'class' => 'Worker',
                'function' => 'handle',
                'args' => [42, 1.5, null, true, false, 'abcdefghijk', [], ['value'], new stdClass()],
            ])
        );
    }

    public function testTraceSelectionAndSourcePositions(): void
    {
        $trace = [
            ['function' => 'run', 'file' => '/project/worker.php', 'line' => 12],
            ['function' => 'resume'],
        ];
        $this->assertSame([
            ['frame' => 0, 'executing' => 'run()', 'source_position' => '/project/worker.php(12)'],
            ['frame' => 1, 'executing' => 'resume()', 'source_position' => '<internal space>'],
        ], DebuggerHelper::convertTraceToTable($trace));
        $this->assertSame([
            ['frame' => 1, 'executing' => 'resume()', 'source_position' => '<internal space>'],
        ], DebuggerHelper::convertTraceToTable($trace, 1));
    }

    public function testMissingTraceFrameFails(): void
    {
        $this->expectException(DebuggerException::class);
        $this->expectExceptionMessage('No trace info');
        DebuggerHelper::convertTraceToTable([['function' => 'run']], 2);
    }

    public function testTableFormatting(): void
    {
        $this->assertSame('No more content', DebuggerHelper::tableFormat([]));
        $this->assertSame(
            "--------------\n| id | state |\n==============\n| 1  | ready |\n| 2  | null  |\n--------------",
            DebuggerHelper::tableFormat([['id' => 1, 'state' => 'ready'], ['id' => 2, 'state' => null]])
        );
        $table = DebuggerHelper::tableFormat([['value' => str_repeat('x', 520)]]);
        $this->assertStringContainsString(str_repeat('x', 512) . '...', $table);
    }
}
