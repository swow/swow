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

namespace Swow\Tests\Http\Protocol;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;
use Swow\Http\Protocol\H2HeaderBlock;
use Swow\Socket;

/**
 * @internal
 */
#[CoversClass(H2HeaderBlock::class)]
#[CoversClass(H2Connection::class)]
final class H2HeaderBlockTest extends TestCase
{
    public function testHeaderBlockAppendSingleHeadersFrame(): void
    {
        $block = new H2HeaderBlock(1);
        $block->append(new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 1, 'abc'));

        $this->assertTrue($block->isCompleted());
        $this->assertSame('abc', $block->getBuffer());
    }

    public function testHeaderBlockAppendHeadersAndContinuation(): void
    {
        $block = new H2HeaderBlock(1);
        $block->append(new H2Frame(H2Frame::TYPE_HEADERS, 0, 1, 'abc'));
        $block->append(new H2Frame(H2Frame::TYPE_CONTINUATION, H2Frame::FLAG_END_HEADERS, 1, 'def'));

        $this->assertTrue($block->isCompleted());
        $this->assertSame('abcdef', $block->getBuffer());
    }

    public function testHeaderBlockRejectsInterruptingFrame(): void
    {
        $block = new H2HeaderBlock(1);
        $block->append(new H2Frame(H2Frame::TYPE_HEADERS, 0, 1, 'abc'));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Header block was interrupted by a non-continuation frame');

        $block->append(new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'x'));
    }

    public function testHeaderBlockRejectsDifferentStreamContinuation(): void
    {
        $block = new H2HeaderBlock(1);
        $block->append(new H2Frame(H2Frame::TYPE_HEADERS, 0, 1, 'abc'));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('CONTINUATION frame stream does not match active header block');

        $block->append(new H2Frame(H2Frame::TYPE_CONTINUATION, H2Frame::FLAG_END_HEADERS, 3, 'def'));
    }

    public function testReadHeaderBlockCollectsUntilEndHeaders(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, 0, 1, 'abc'),
            new H2Frame(H2Frame::TYPE_CONTINUATION, H2Frame::FLAG_END_HEADERS, 1, 'def'),
        ];

        $block = $connection->readHeaderBlock();

        $this->assertSame(1, $block->getStreamId());
        $this->assertSame('abcdef', $block->getBuffer());
        $this->assertTrue($block->isCompleted());
    }
}
