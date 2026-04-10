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
use Swow\Http\Protocol\H2Headers;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Socket;

/**
 * @internal
 */
#[CoversClass(H2Headers::class)]
#[CoversClass(H2ServerConnection::class)]
#[CoversClass(H2Connection::class)]
final class H2ServerConnectionTest extends TestCase
{
    public function testRecvRequestFromHeadersCreatesServerRequest(): void
    {
        $connection = new H2ServerConnection();
        $request = $connection->recvRequestFromHeaders([
            [':method', 'GET'],
            [':scheme', 'https'],
            [':authority', 'example.com'],
            [':path', '/hello?foo=bar'],
            ['accept', 'application/json'],
            ['cookie', 'a=1; b=2'],
        ]);

        $this->assertInstanceOf(ServerRequest::class, $request);
        $this->assertSame('GET', $request->getMethod());
        $this->assertSame('https://example.com/hello?foo=bar', (string) $request->getUri());
        $this->assertSame('application/json', $request->getHeaderLine('accept'));
        $this->assertSame('example.com', $request->getHeaderLine('host'));
        $this->assertSame('1', $request->getCookieParams()['a']);
        $this->assertSame('2', $request->getCookieParams()['b']);
        $this->assertSame('2.0', $request->getProtocolVersion());
    }

    public function testRecvRequestFromHeadersRejectsMissingPseudoHeader(): void
    {
        $connection = new H2ServerConnection();

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Missing required pseudo-header ":method"');

        $connection->recvRequestFromHeaders([
            [':scheme', 'https'],
            [':authority', 'example.com'],
            [':path', '/'],
        ]);
    }

    public function testRecvRequestFromHeadersSupportsConnectWithoutPath(): void
    {
        $connection = new H2ServerConnection();
        $request = $connection->recvRequestFromHeaders([
            [':method', 'CONNECT'],
            [':authority', 'example.com:443'],
        ]);

        $this->assertSame('CONNECT', $request->getMethod());
        $this->assertSame('example.com:443', $request->getHeaderLine('host'));
    }

    public function testTryRecvStreamRequestReturnsNullWhenNoMoreRequestStreamsAllowed(): void
    {
        $queuedConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 0, H2Exception::ERROR_NO_ERROR)));
            }
        };

        $connection = new H2ServerConnection(connection: $queuedConnection);

        $this->assertNull($connection->tryRecvStreamRequest());
    }

    public function testSendResponseHeadersAndDataWriteFrames(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['content-type' => 'text/plain']);
        $connection->sendResponseData(1, 'hello', true);

        $this->assertCount(2, $recordingConnection->frames);
        $headersFrame = $recordingConnection->frames[0];
        $dataFrame = $recordingConnection->frames[1];

        $this->assertSame(H2Frame::TYPE_HEADERS, $headersFrame->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS, $headersFrame->flags);
        $this->assertSame(1, $headersFrame->streamId);
        $this->assertNotSame('', $headersFrame->payload);

        $this->assertSame(H2Frame::TYPE_DATA, $dataFrame->type);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $dataFrame->flags);
        $this->assertSame(1, $dataFrame->streamId);
        $this->assertSame('hello', $dataFrame->payload);
    }

    public function testSendResponseDataSplitsPayloadLargerThanSingleFrameLimit(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $payload = str_repeat('a', H2Frame::DEFAULT_MAX_FRAME_SIZE + 1);

        $connection->sendResponseData(1, $payload, true);

        $this->assertCount(2, $recordingConnection->frames);

        $firstFrame = $recordingConnection->frames[0];
        $lastFrame = $recordingConnection->frames[1];

        $this->assertSame(H2Frame::TYPE_DATA, $firstFrame->type);
        $this->assertSame(0, $firstFrame->flags);
        $this->assertSame(H2Frame::DEFAULT_MAX_FRAME_SIZE, strlen($firstFrame->payload));

        $this->assertSame(H2Frame::TYPE_DATA, $lastFrame->type);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $lastFrame->flags);
        $this->assertSame(1, strlen($lastFrame->payload));
        $this->assertSame($payload, $firstFrame->payload . $lastFrame->payload);
    }

    public function testSendResponseHeadersUsesNegotiatedMaxFrameSize(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x5, 32768)));

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-long' => str_repeat('a', 20000)]);

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[0]->type);
        $this->assertSame(1, $recordingConnection->frames[0]->streamId);
    }

    public function testSendResponseHeadersSplitsPayloadExceedingNegotiatedMaxFrameSize(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x5, 20000)));

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-long' => str_repeat('b', 21000)]);

        $this->assertCount(2, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[0]->type);
        $this->assertSame(1, $recordingConnection->frames[0]->streamId);
        $this->assertSame(0, $recordingConnection->frames[0]->flags & H2Frame::FLAG_END_HEADERS);
        $this->assertSame(H2Frame::TYPE_CONTINUATION, $recordingConnection->frames[1]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS, $recordingConnection->frames[1]->flags);
    }

    public function testSendResponseHeadersSetsEndStreamOnlyOnFirstHeadersFrameWhenSplit(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x5, 20000)));

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-long' => str_repeat('c', 21000)], true);

        $this->assertCount(2, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[0]->type);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->frames[0]->flags & H2Frame::FLAG_END_STREAM);
        $this->assertSame(H2Frame::TYPE_CONTINUATION, $recordingConnection->frames[1]->type);
        $this->assertSame(0, $recordingConnection->frames[1]->flags & H2Frame::FLAG_END_STREAM);
        $this->assertSame(H2Frame::FLAG_END_HEADERS, $recordingConnection->frames[1]->flags);
    }

    public function testSendResponseHeadersDoesNotConsumeSendWindows(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 100;
                $this->streamSendWindows[1] = 100;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }

            public function exposeConnectionSendWindow(): int
            {
                return $this->connectionSendWindow;
            }

            public function exposeStreamSendWindow(int $streamId): int
            {
                return $this->streamSendWindows[$streamId] ?? self::DEFAULT_INITIAL_WINDOW_SIZE;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-a' => 'b']);

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame(100, $recordingConnection->exposeConnectionSendWindow());
        $this->assertSame(100, $recordingConnection->exposeStreamSendWindow(1));
    }

    public function testSendResponseHeadersIgnoresConnectionSendWindow(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 1;
                $this->streamSendWindows[1] = 100;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-a' => 'b']);

        $this->assertCount(1, $recordingConnection->frames);
    }

    public function testSendResponseHeadersIgnoresStreamSendWindow(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 100;
                $this->streamSendWindows[1] = 1;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-a' => 'b']);

        $this->assertCount(1, $recordingConnection->frames);
    }

    public function testSendResponseHeadersAllowsNewStreamWhenInitialWindowSizeIsZero(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 0)));

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(3, 200, ['x-a' => 'b']);

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame(3, $recordingConnection->frames[0]->streamId);
    }

    public function testSendResponseHeadersAllowsSmallHeaderBlockWithinReducedInitialWindowSize(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 20)));

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(5, 200, ['x-a' => 'b']);

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[0]->type);
        $this->assertSame(5, $recordingConnection->frames[0]->streamId);
    }

    public function testSendResponseTrailersSendsHeadersEndStream(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseTrailers(1, ['x-trailer' => 'done']);

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[0]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $recordingConnection->frames[0]->flags);
    }

    public function testSendResponseTrailersRejectsPseudoHeaders(): void
    {
        $connection = new H2ServerConnection(connection: new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {});

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Trailer headers must not contain pseudo-headers');

        $connection->sendResponseTrailers(1, [':status' => '200']);
    }
}
