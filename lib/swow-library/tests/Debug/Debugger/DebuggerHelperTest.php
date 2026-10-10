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

    public function testSourceExcerptAtBeginningOfFile(): void
    {
        $filename = tempnam(sys_get_temp_dir(), 'swow_debug_source');
        file_put_contents($filename, "first\nsecond\nthird\nfourth\n");
        try {
            $source = null;
            $this->assertSame([
                ['line' => '1->', 'contents' => 'first'],
                ['line' => 2, 'contents' => 'second'],
            ], DebuggerHelper::getSourceFileContentAsTable($filename, 1, $source, 2));
            $this->assertNotNull($source);
            $this->assertSame([
                ['line' => 3, 'contents' => 'third'],
            ], DebuggerHelper::getFollowingSourceFileContent($source, 2, 2, 0));
        } finally {
            unset($source);
            unlink($filename);
        }
    }

    public function testSourceExcerptKeepsContextAndFollowingPosition(): void
    {
        $filename = tempnam(sys_get_temp_dir(), 'swow_debug_source');
        file_put_contents($filename, implode("\n", range(1, 20)));
        try {
            $source = null;
            $rows = DebuggerHelper::getSourceFileContentAsTable($filename, 10, $source);
            $this->assertSame([7, 8, 9, '10->', 11, 12, 13, 14], array_column($rows, 'line'));
            $this->assertSame(array_map(strval(...), range(7, 14)), array_column($rows, 'contents'));
            $following = DebuggerHelper::getFollowingSourceFileContent($source, 10, 3);
            $this->assertSame([
                ['line' => 15, 'contents' => '15'],
                ['line' => 16, 'contents' => '16'],
            ], $following);
            $rows = DebuggerHelper::getSourceFileContentAsTable($filename, 3, $source);
            $this->assertSame([1, 2, '3->', 4, 5, 6, 7, 8], array_column($rows, 'line'));
            $this->expectException(DebuggerException::class);
            $this->expectExceptionMessage('File Line not found');
            DebuggerHelper::getSourceFileContentAsTable($filename, 40, $source);
        } finally {
            unset($source);
            unlink($filename);
        }
    }
}
