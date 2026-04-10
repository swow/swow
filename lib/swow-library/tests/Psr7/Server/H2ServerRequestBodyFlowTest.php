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

namespace Swow\Tests\Psr7\Server;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Socket;

/**
 * @internal
 */
#[CoversClass(H2ServerConnection::class)]
#[CoversClass(H2Connection::class)]
final class H2ServerRequestBodyFlowTest extends TestCase
{
    public function testRecvRequestWithoutBodyWhenHeadersEndStreamIsSet(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x01/"
            ),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $request = $connection->recvRequest();

        $this->assertSame('', (string) $request->getBody());
    }

    public function testRecvRequestWithSingleDataFrameBody(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'hello'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $request = $connection->recvRequest();

        $this->assertSame('hello', (string) $request->getBody());
    }

    public function testRecvStreamRequestWithBodyReturnsStreamIdAndBody(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'hello'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $result = $connection->recvStreamRequest();

        $this->assertSame(1, $result['streamId']);
        $this->assertSame('hello', (string) $result['request']->getBody());
    }

    public function testRecvRequestWithMultipleDataFramesBody(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'hel'),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'lo'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $request = $connection->recvRequest();

        $this->assertSame('hello', (string) $request->getBody());
    }

    public function testRecvRequestDefersDifferentStreamFramesUntilCurrentBodyCompletes(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                3,
                "\x82\x87\x41\x0bexample.com\x44\x05/next"
            ),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'oops'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $firstRequest = $connection->recvRequest();
        $secondRequest = $connection->recvRequest();

        $this->assertSame('oops', (string) $firstRequest->getBody());
        $this->assertSame('/next', $secondRequest->getUri()->getPath());
    }

    public function testRecvRequestDefersDifferentStreamHeaderBlockUntilCurrentBodyCompletes(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                0,
                3,
                "\x82\x87"
            ),
            new H2Frame(
                H2Frame::TYPE_CONTINUATION,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                3,
                "\x41\x0bexample.com\x44\x05/next"
            ),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'body'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $firstRequest = $connection->recvRequest();
        $secondRequest = $connection->recvRequest();

        $this->assertSame('body', (string) $firstRequest->getBody());
        $this->assertSame('/next', $secondRequest->getUri()->getPath());
    }

    public function testRecvRequestSendsRstStreamOnStreamLevelBodyError(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];
            /** @var list<H2Frame> */
            public array $sentFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->sentFrames[] = $frame;
            }
        };
        $queuedConnection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 2)));
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'abc'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Stream receive window exhausted');

        try {
            $connection->recvRequest();
        } finally {
            $this->assertCount(1, $queuedConnection->sentFrames);
            $this->assertSame(H2Frame::TYPE_RST_STREAM, $queuedConnection->sentFrames[0]->type);
            $this->assertSame(1, $queuedConnection->sentFrames[0]->streamId);
            $this->assertSame(pack('N', H2Exception::ERROR_FLOW_CONTROL_ERROR), $queuedConnection->sentFrames[0]->payload);
        }
    }

    public function testRecvRequestAllowsBodyDrainForExistingStreamAfterGoAway(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }
        };
        $queuedConnection->markRequestStreamProcessed(1);
        $queuedConnection->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)));
        $queuedConnection->frames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'hello'),
        ];

        $body = $queuedConnection->readRequestBody(1);

        $this->assertSame('hello', $body);
    }

    public function testReadRequestBodyRejectsWhenRemoteSideAlreadyClosed(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }
        };
        $queuedConnection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82"),
        ];
        $queuedConnection->readHeaderBlock();

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Cannot receive DATA on remotely closed stream');

        $queuedConnection->readRequestBody(1);
    }

    public function testRecvRequestStoresTrailingHeadersInRequestAttributes(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'hello'),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x40\x0ax-trailing\x03yes"),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $request = $connection->recvRequest();

        $this->assertSame('hello', (string) $request->getBody());
        $this->assertSame(['x-trailing' => ['yes']], H2ServerConnection::getRequestTrailers($request));
    }

    public function testRecvRequestRejectsTrailingPseudoHeaders(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'hello'),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x84"),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Trailer headers must not contain pseudo-headers');

        $connection->recvRequest();
    }

    public function testRecvRequestDefersDifferentStreamHeaderBlockUntilTrailersComplete(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'hello'),
            new H2Frame(H2Frame::TYPE_HEADERS, 0, 1, "\x40\x0ax-trailing"),
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                3,
                "\x82\x87\x41\x0bexample.com\x44\x05/next"
            ),
            new H2Frame(H2Frame::TYPE_CONTINUATION, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x03yes"),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $firstRequest = $connection->recvRequest();
        $secondRequest = $connection->recvRequest();

        $this->assertSame('hello', (string) $firstRequest->getBody());
        $this->assertSame(['x-trailing' => ['yes']], H2ServerConnection::getRequestTrailers($firstRequest));
        $this->assertSame('/next', $secondRequest->getUri()->getPath());
    }

    public function testRecvRequestRestoresDeferredSplitHeaderBlockAfterControlFramesArePumped(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];
            /** @var list<H2Frame> */
            public array $sentFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->sentFrames[] = $frame;
            }
        };
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_HEADERS, 0, 3, "\x82\x87"),
            new H2Frame(H2Frame::TYPE_CONTINUATION, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x41\x0bexample.com\x44\x05/next"),
            new H2Frame(H2Frame::TYPE_PING, 0, 0, '12345678'),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'body'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $firstRequest = $connection->recvRequest();
        $secondRequest = $connection->recvRequest();

        $this->assertSame('body', (string) $firstRequest->getBody());
        $this->assertSame('/next', $secondRequest->getUri()->getPath());
        $this->assertCount(1, $queuedConnection->sentFrames);
        $this->assertSame(H2Frame::TYPE_PING, $queuedConnection->sentFrames[0]->type);
        $this->assertSame(H2Frame::FLAG_ACK, $queuedConnection->sentFrames[0]->flags);
    }

    public function testRecvRequestRejectsTrailerBlockInterruptedByControlFrame(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'hello'),
            new H2Frame(H2Frame::TYPE_HEADERS, 0, 1, "\x40\x0ax-trailing"),
            new H2Frame(H2Frame::TYPE_PING, 0, 0, '12345678'),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('CONTINUATION frame stream does not match active header block');

        $connection->recvRequest();
    }

    public function testRecvRequestRejectsTrailerBlockInterruptedByWindowUpdate(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'hello'),
            new H2Frame(H2Frame::TYPE_HEADERS, 0, 1, "\x40\x0ax-trailing"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1024)),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('CONTINUATION frame stream does not match active header block');

        $connection->recvRequest();
    }

    public function testReadRequestBodyAllowsExistingStreamTailAfterGoAwayWhileQueuedNewStreamIsDropped(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }
        };
        $queuedConnection->markRequestStreamProcessed(1);
        $queuedConnection->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)));
        $queuedConnection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x05/dropme"),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'hello'),
        ];

        $body = $queuedConnection->readRequestBody(1);

        $this->assertSame('hello', $body);
        $this->assertNull($queuedConnection->readRequestStartFrame());
    }
}
