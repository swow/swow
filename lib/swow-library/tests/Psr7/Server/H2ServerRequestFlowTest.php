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
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Socket;

/**
 * @internal
 */
#[CoversClass(H2ServerConnection::class)]
#[CoversClass(H2Connection::class)]
final class H2ServerRequestFlowTest extends TestCase
{
    public function testRecvRequestBuildsServerRequestFromQueuedFrames(): void
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
                "\x82" .                 // :method GET
                "\x87" .                 // :scheme https
                "\x41\x0bexample.com" .  // :authority: example.com
                "\x44\x06/hello"         // :path: /hello
            ),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $request = $connection->recvRequest();

        $this->assertInstanceOf(ServerRequest::class, $request);
        $this->assertSame('GET', $request->getMethod());
        $this->assertSame('https://example.com/hello', (string) $request->getUri());
        $this->assertSame('example.com', $request->getHeaderLine('host'));
        $this->assertSame('2.0', $request->getProtocolVersion());
    }

    public function testRecvStreamRequestReturnsStreamIdAndRequest(): void
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
                "\x82\x87\x41\x0bexample.com\x44\x06/hello"
            ),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $result = $connection->recvStreamRequest();

        $this->assertSame(1, $result['streamId']);
        $this->assertInstanceOf(ServerRequest::class, $result['request']);
        $this->assertSame('GET', $result['request']->getMethod());
    }

    public function testRecvStreamRequestPumpsSettingsBeforeHeadersAndSendsAck(): void
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
            new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x5, 32768)),
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x06/hello"
            ),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $result = $connection->recvStreamRequest();

        $this->assertSame(1, $result['streamId']);
        $this->assertCount(1, $queuedConnection->sentFrames);
        $this->assertSame(H2Frame::TYPE_SETTINGS, $queuedConnection->sentFrames[0]->type);
        $this->assertSame(H2Frame::FLAG_ACK, $queuedConnection->sentFrames[0]->flags);
    }

    public function testRecvStreamRequestPumpsPingBeforeHeadersAndSendsAck(): void
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
            new H2Frame(H2Frame::TYPE_PING, 0, 0, '12345678'),
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x06/hello"
            ),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);
        $result = $connection->recvStreamRequest();

        $this->assertSame(1, $result['streamId']);
        $this->assertCount(1, $queuedConnection->sentFrames);
        $this->assertSame(H2Frame::TYPE_PING, $queuedConnection->sentFrames[0]->type);
        $this->assertSame(H2Frame::FLAG_ACK, $queuedConnection->sentFrames[0]->flags);
        $this->assertSame('12345678', $queuedConnection->sentFrames[0]->payload);
    }

    public function testRecvRequestRejectsEmptyHeadersPayload(): void
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
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 1, ''),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('HEADERS frame payload must not be empty');

        $connection->recvRequest();
    }

    public function testRecvRequestRejectsNewStreamAfterGoAwayAndSendsRstStream(): void
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
        $queuedConnection->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)));
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                3,
                "\x82\x87\x41\x0bexample.com\x44\x06/hello"
            ),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('New stream is not allowed after GOAWAY');

        try {
            $connection->recvRequest();
        } finally {
            $this->assertCount(1, $queuedConnection->sentFrames);
            $this->assertSame(H2Frame::TYPE_RST_STREAM, $queuedConnection->sentFrames[0]->type);
            $this->assertSame(3, $queuedConnection->sentFrames[0]->streamId);
            $this->assertSame(pack('N', H2Exception::ERROR_REFUSED_STREAM), $queuedConnection->sentFrames[0]->payload);
        }
    }

    public function testRecvRequestRejectsHeadersForAlreadyClosedStream(): void
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
        $queuedConnection->sendRstStream(1, H2Exception::ERROR_CANCEL);
        $queuedConnection->frames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x06/hello"
            ),
        ];

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Frame received for closed stream');

        $connection->recvRequest();
    }
}
