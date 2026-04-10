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
use Swow\Socket;

/**
 * @internal
 */
#[CoversClass(H2Connection::class)]
final class H2RequestBodyProtocolTest extends TestCase
{
    public function testReadRequestBodyAllowsEmptyDataFrameWithEndStream(): void
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
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, ''),
        ];

        $this->assertSame('', $connection->readRequestBody(1));
    }

    public function testReadRequestBodyDefersDifferentStreamFrames(): void
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
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                3,
                "\x82\x87\x41\x0bexample.com\x45\x04/next"
            ),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'body'),
        ];

        $this->assertSame('body', $connection->readRequestBody(1));
        $nextRequest = $connection->readHeaderBlock();
        $this->assertSame(3, $nextRequest->getStreamId());
    }

    public function testReadRequestBodyWithTrailersReturnsTrailerBlock(): void
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
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'body'),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, 'trailers'),
        ];

        $result = $connection->readRequestBodyWithTrailers(1);

        $this->assertSame('body', $result['body']);
        $this->assertNotNull($result['trailers']);
        $this->assertSame('trailers', $result['trailers']?->getBuffer());
    }

    public function testReadRequestBodyRejectsNonDataFrame(): void
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
            new H2Frame(H2Frame::TYPE_CONTINUATION, 0, 1, 'bad'),
        ];

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Expected DATA or trailing HEADERS frame while reading request body');

        $connection->readRequestBody(1);
    }

    public function testReadRequestBodyRejectsTrailingHeadersWithoutEndStream(): void
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
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 1, 'trailers'),
        ];

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Trailing HEADERS frame must end the request stream');

        $connection->readRequestBodyWithTrailers(1);
    }
}
