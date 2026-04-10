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
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\H2ResponseEnvelope;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Socket;
use Swow\SocketException;

use function array_map;
use function implode;
use function str_repeat;
use function strlen;

/**
 * @internal
 */
#[CoversClass(H2ServerConnection::class)]
final class H2ServerResponseFlowTest extends TestCase
{
    public function testHandleOneRequestSendsHeadersOnlyForEmptyBodyResponse(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamReceiveWindows[1] = 2;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $streamId = $connection->handleOneRequest(
            static fn($request) => Psr7::createResponse(204)
        );

        $this->assertSame(1, $streamId);
        $this->assertCount(1, $recordingConnection->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[0]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $recordingConnection->writtenFrames[0]->flags);
    }

    public function testHandleOneRequestSendsHeadersThenDataForBodyResponse(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamReceiveWindows[1] = 2;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $streamId = $connection->handleOneRequest(
            static fn($request) => Psr7::createResponse(200, headers: ['x-a' => 'b'], body: 'hello')
        );

        $this->assertSame(1, $streamId);
        $this->assertCount(2, $recordingConnection->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[0]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS, $recordingConnection->writtenFrames[0]->flags);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->writtenFrames[1]->type);
        $this->assertSame('hello', $recordingConnection->writtenFrames[1]->payload);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->writtenFrames[1]->flags);
    }

    public function testHandleOneRequestSupportsResponseEnvelopeWithTrailers(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->handleOneRequest(
            static fn($request) => H2ResponseEnvelope::from(
                Psr7::createResponse(200, body: 'ok'),
                ['x-trailer' => 'done']
            )
        );

        $this->assertCount(3, $recordingConnection->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->writtenFrames[1]->type);
        $this->assertSame(0, $recordingConnection->writtenFrames[1]->flags);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[2]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $recordingConnection->writtenFrames[2]->flags);
    }

    public function testHandleRequestsProcessesElevenStreamsWithBodiesAndBidirectionalTrailers(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };

        foreach (range(1, 11) as $index) {
            $streamId = ($index * 2) - 1;
            $path = '/s' . $index;
            $body = 'body-' . $index;
            $trailerValue = 't' . $index;

            $recordingConnection->readFrames[] = new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                $streamId,
                "\x82\x87\x41\x0bexample.com\x44" . chr(strlen($path)) . $path
            );
            $recordingConnection->readFrames[] = new H2Frame(H2Frame::TYPE_DATA, 0, $streamId, $body);
            $recordingConnection->readFrames[] = new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM,
                $streamId,
                "\x40\x10x-client-trailer" . chr(strlen($trailerValue)) . $trailerValue
            );
        }

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static function ($request, int $streamId): H2ResponseEnvelope {
                $index = intdiv($streamId + 1, 2);

                return H2ResponseEnvelope::from(
                    response: Psr7::createResponse(200, body: 'resp-' . $index),
                    trailers: ['x-server-trailer' => 'done-' . $index]
                );
            },
            limit: 11
        );

        $this->assertSame(11, $handled);
        $this->assertCount(33, $recordingConnection->writtenFrames);
        foreach (range(0, 10) as $offset) {
            $frameBase = $offset * 3;
            $streamId = (($offset + 1) * 2) - 1;
            $index = $offset + 1;
            $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[$frameBase]->type);
            $this->assertSame($streamId, $recordingConnection->writtenFrames[$frameBase]->streamId);
            $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->writtenFrames[$frameBase + 1]->type);
            $this->assertSame('resp-' . $index, $recordingConnection->writtenFrames[$frameBase + 1]->payload);
            $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[$frameBase + 2]->type);
            $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $recordingConnection->writtenFrames[$frameBase + 2]->flags);
        }
    }

    public function testHandleOneRequestAcceptsArrayResponseWithTrailingHeaders(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->handleOneRequest(
            static fn($request): array => [201, ['x-header' => 'yes'], 'ok', ['x-trailer' => 'done']]
        );

        $this->assertCount(3, $recordingConnection->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->writtenFrames[1]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[2]->type);
    }

    public function testHandleOneRequestAcceptsNamedResponseMapWithTrailers(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->handleOneRequest(
            static fn($request): array => [
                'status' => 202,
                'headers' => ['x-header' => 'yes'],
                'body' => 'ok',
                'trailers' => ['x-trailer' => 'done'],
            ]
        );

        $this->assertCount(3, $recordingConnection->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->writtenFrames[1]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[2]->type);
    }

    public function testHandleOneRequestDoesNotSwallowHandlerException(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);

        $this->expectException(\RuntimeException::class);
        $this->expectExceptionMessage('boom');

        try {
            $connection->handleOneRequest(
                static function ($request) {
                    throw new \RuntimeException('boom');
                }
            );
        } finally {
            $this->assertCount(0, $recordingConnection->writtenFrames);
        }
    }

    public function testHandleRequestsProcessesMultipleRequestsSequentially(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x02/b"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: 2
        );

        $this->assertSame(2, $handled);
        $this->assertCount(2, $recordingConnection->writtenFrames);
        $this->assertSame(1, $recordingConnection->writtenFrames[0]->streamId);
        $this->assertSame(3, $recordingConnection->writtenFrames[1]->streamId);
    }

    public function testHandleRequestsContinuesAfterStreamLevelError(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamReceiveWindows[1] = 2;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 1, "\x82\x87\x41\x0bexample.com\x44\x05/echo"),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'abc'),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/ok"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: 1
        );

        $this->assertSame(1, $handled);
        $this->assertCount(2, $recordingConnection->writtenFrames);
        $this->assertSame(H2Frame::TYPE_RST_STREAM, $recordingConnection->writtenFrames[0]->type);
        $this->assertSame(1, $recordingConnection->writtenFrames[0]->streamId);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[1]->type);
        $this->assertSame(5, $recordingConnection->writtenFrames[1]->streamId);
    }

    public function testHandleRequestsHonorsLimit(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x02/b"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: 1
        );

        $this->assertSame(1, $handled);
        $this->assertCount(1, $recordingConnection->writtenFrames);
        $this->assertCount(1, $recordingConnection->readFrames);
    }

    public function testHandleRequestsReturnsZeroWhenLimitIsZero(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: 0
        );

        $this->assertSame(0, $handled);
        $this->assertCount(0, $recordingConnection->writtenFrames);
        $this->assertCount(1, $recordingConnection->readFrames);
    }

    public function testHandleRequestsContinuesAfterHandlerThrowsStreamLevelH2Exception(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x02/b"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $calls = 0;
        $handled = $connection->handleRequests(
            static function ($request, int $streamId) use (&$calls) {
                $calls++;
                if ($calls === 1) {
                    throw H2Exception::forStreamError($streamId, 'handler stream error', H2Exception::ERROR_CANCEL);
                }

                return Psr7::createResponse(204);
            },
            limit: 1
        );

        $this->assertSame(1, $handled);
        $this->assertCount(2, $recordingConnection->writtenFrames);
        $this->assertSame(H2Frame::TYPE_RST_STREAM, $recordingConnection->writtenFrames[0]->type);
        $this->assertSame(1, $recordingConnection->writtenFrames[0]->streamId);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->writtenFrames[1]->type);
        $this->assertSame(3, $recordingConnection->writtenFrames[1]->streamId);
    }

    public function testHandleRequestsStopsWhenHandlerThrowsConnectionLevelH2Exception(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('handler connection error');

        try {
            $connection->handleRequests(
                static fn($request, int $streamId) => throw H2Exception::forConnectionError('handler connection error'),
                limit: 1
            );
        } finally {
            $this->assertCount(0, $recordingConnection->writtenFrames);
        }
    }

    public function testHandleRequestsProcessesWindowUpdateBetweenRequests(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 64;
                $this->streamSendWindows[1] = 64;
                $this->streamSendWindows[3] = 64;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 16)),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x02/b"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: 2
        );

        $this->assertSame(2, $handled);
        $this->assertCount(2, $recordingConnection->writtenFrames);
        $this->assertSame(1, $recordingConnection->writtenFrames[0]->streamId);
        $this->assertSame(3, $recordingConnection->writtenFrames[1]->streamId);
    }

    public function testHandleRequestsStopsNaturallyAfterGoAwayFinishesAllowedRange(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
            new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: null
        );

        $this->assertSame(1, $handled);
        $this->assertCount(1, $recordingConnection->writtenFrames);
        $this->assertSame(1, $recordingConnection->writtenFrames[0]->streamId);
    }

    public function testHandleRequestsStopsNaturallyWhenOnlyGoAwayIsReceived(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 0, H2Exception::ERROR_NO_ERROR)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: null
        );

        $this->assertSame(0, $handled);
        $this->assertCount(0, $recordingConnection->writtenFrames);
    }

    public function testHandleRequestsStopsNaturallyAfterOneRequestThenEof(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if ($frame instanceof H2Frame) {
                    return $frame;
                }

                throw new SocketException('Connection has been closed');
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x02/a"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: null
        );

        $this->assertSame(1, $handled);
        $this->assertCount(1, $recordingConnection->writtenFrames);
    }

    public function testHandleRequestsReturnsZeroOnImmediateEof(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                throw new SocketException('Connection has been closed');
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->writtenFrames[] = $frame;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $handled = $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: null
        );

        $this->assertSame(0, $handled);
        $this->assertCount(0, $recordingConnection->writtenFrames);
    }

    public function testHandleRequestsStillThrowsNonEofReadException(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function readFrame(?int $timeout = null): H2Frame
            {
                throw new \RuntimeException('boom');
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);

        $this->expectException(\RuntimeException::class);
        $this->expectExceptionMessage('boom');

        $connection->handleRequests(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            limit: null
        );
    }

    public function testSendResponseSplitsLargeHeadersAndBodyTogether(): void
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
        $response = Psr7::createResponse(200, headers: ['x-long' => str_repeat('z', 21000)], body: str_repeat('a', 21000));

        $connection->sendResponse(1, $response);

        $this->assertCount(4, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[0]->type);
        $this->assertSame(H2Frame::TYPE_CONTINUATION, $recordingConnection->frames[1]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->frames[2]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->frames[3]->type);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->frames[3]->flags);
        $this->assertSame(20000, strlen($recordingConnection->frames[2]->payload));
        $this->assertSame(1000, strlen($recordingConnection->frames[3]->payload));
    }

    public function testSendResponseDataUsesSingleFrameForSmallPayload(): void
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
        $connection->sendResponseData(1, 'hello', true);

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame('hello', $recordingConnection->frames[0]->payload);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->frames[0]->flags);
    }

    public function testSendResponseDataSplitsLargePayloadIntoMultipleFrames(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };

        $payload = str_repeat('a', H2Frame::DEFAULT_MAX_FRAME_SIZE + 10);
        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, $payload, true);

        $this->assertCount(2, $recordingConnection->frames);
        $this->assertSame(H2Frame::DEFAULT_MAX_FRAME_SIZE, strlen($recordingConnection->frames[0]->payload));
        $this->assertSame(10, strlen($recordingConnection->frames[1]->payload));
        $this->assertSame(0, $recordingConnection->frames[0]->flags);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->frames[1]->flags);
        $this->assertSame(
            $payload,
            implode('', array_map(static fn(H2Frame $frame): string => $frame->payload, $recordingConnection->frames))
        );
    }

    public function testSendResponseDataUsesNegotiatedMaxFrameSize(): void
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

        $payload = str_repeat('b', 32769);
        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, $payload, true);

        $this->assertCount(2, $recordingConnection->frames);
        $this->assertSame(32768, strlen($recordingConnection->frames[0]->payload));
        $this->assertSame(1, strlen($recordingConnection->frames[1]->payload));
        $this->assertSame(0, $recordingConnection->frames[0]->flags);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->frames[1]->flags);
    }

    public function testSendResponseDataConsumesSendWindowsAndSplitsBySmallestWindow(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 4;
                $this->streamSendWindows[1] = 3;
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
        $connection->sendResponseData(1, 'abc', true);

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame('abc', $recordingConnection->frames[0]->payload);
        $this->assertSame(1, $recordingConnection->exposeConnectionSendWindow());
        $this->assertSame(0, $recordingConnection->exposeStreamSendWindow(1));
    }

    public function testSendResponseDataRejectsWhenConnectionSendWindowIsZero(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 0;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);

        $this->expectException(\Swow\Http\Protocol\H2Exception::class);
        $this->expectExceptionMessage('Connection send window exhausted');

        $connection->sendResponseData(1, 'a', true);
    }

    public function testSendResponseDataRejectsWhenStreamSendWindowIsZero(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }
        };

        $connection = new H2ServerConnection(connection: $recordingConnection);

        $this->expectException(\Swow\Http\Protocol\H2Exception::class);
        $this->expectExceptionMessage('Stream send window exhausted');

        $connection->sendResponseData(1, 'a', true);
    }

    public function testSendResponseDataUsesUpdatedInitialWindowSizeForNewStreamSendWindow(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 2)));

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $this->expectException(\Swow\Http\Protocol\H2Exception::class);
        $this->expectExceptionMessage('Stream send window exhausted');

        try {
            $connection->sendResponseData(3, 'abc', true);
        } finally {
            $this->assertCount(1, $recordingConnection->frames);
            $this->assertSame('ab', $recordingConnection->frames[0]->payload);
        }
    }

    public function testSendResponseDataWaitsForWindowUpdateAndResumesInSingleCall(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 2;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'abcd', true);

        $this->assertCount(2, $recordingConnection->frames);
        $this->assertSame('ab', $recordingConnection->frames[0]->payload);
        $this->assertSame('cd', $recordingConnection->frames[1]->payload);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->frames[1]->flags);
    }

    public function testSendResponseDataWaitFailsWhenRstStreamArrives(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            H2Frame::createRstStream(1, H2Exception::ERROR_CANCEL),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Cannot send on locally closed stream');

        $connection->sendResponseData(1, 'body', true);
    }

    public function testSendResponseDataWaitKeepsQueuedRequestAfterRstStreamInterrupt(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x05/next"),
            H2Frame::createRstStream(1, H2Exception::ERROR_CANCEL),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);

        try {
            $connection->sendResponseData(1, 'body', true);
            $this->fail('Expected sendResponseData() to fail after RST_STREAM');
        } catch (H2Exception $exception) {
            $this->assertSame('Cannot send on locally closed stream', $exception->getMessage());
        }

        $streamRequest = $connection->tryRecvStreamRequest();

        $this->assertNotNull($streamRequest);
        $this->assertSame(3, $streamRequest['streamId']);
        $this->assertSame('/next', $streamRequest['request']->getUri()->getPath());
    }

    public function testSendResponseDataWaitPreservesQueuedRequestFrames(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x05/next"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);
        $streamRequest = $connection->tryRecvStreamRequest();

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame('ok', $recordingConnection->frames[0]->payload);
        $this->assertNotNull($streamRequest);
        $this->assertSame(3, $streamRequest['streamId']);
        $this->assertSame('/next', $streamRequest['request']->getUri()->getPath());
    }

    public function testSendResponseDataWaitPreservesMultipleQueuedRequestFramesInOrder(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x03/aa"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/bb"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);
        $first = $connection->tryRecvStreamRequest();
        $second = $connection->tryRecvStreamRequest();

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame('ok', $recordingConnection->frames[0]->payload);
        $this->assertSame(3, $first['streamId']);
        $this->assertSame('/aa', $first['request']->getUri()->getPath());
        $this->assertSame(5, $second['streamId']);
        $this->assertSame('/bb', $second['request']->getUri()->getPath());
    }

    public function testSendResponseDataWaitPreservesDeferredHeaderBlocksAcrossStreams(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, 0, 3, "\x82\x87"),
            new H2Frame(H2Frame::TYPE_CONTINUATION, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x41\x0bexample.com\x44\x03/aa"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/bb"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);
        $first = $connection->tryRecvStreamRequest();
        $second = $connection->tryRecvStreamRequest();

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame('ok', $recordingConnection->frames[0]->payload);
        $this->assertSame(3, $first['streamId']);
        $this->assertSame('/aa', $first['request']->getUri()->getPath());
        $this->assertSame(5, $second['streamId']);
        $this->assertSame('/bb', $second['request']->getUri()->getPath());
    }

    public function testPendingControlFramesArePumpedBeforeDeferredRequestStartFrames(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x03/aa"),
            new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('New stream is not allowed after GOAWAY');

        $connection->tryRecvStreamRequest();
    }

    public function testPendingGoAwayStillAllowsDeferredRequestWithinAllowedRange(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x03/ok"),
            new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 3, H2Exception::ERROR_NO_ERROR)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);
        $streamRequest = $connection->tryRecvStreamRequest();

        $this->assertNotNull($streamRequest);
        $this->assertSame(3, $streamRequest['streamId']);
        $this->assertSame('/ok', $streamRequest['request']->getUri()->getPath());
    }

    public function testSendResponseDataWaitHandlesGoAwayWindowUpdateAndRstStreamSequence(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x05/next"),
            new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 3, H2Exception::ERROR_NO_ERROR)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
            H2Frame::createRstStream(1, H2Exception::ERROR_CANCEL),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);

        try {
            $connection->sendResponseData(1, 'abcd', true);
            $this->fail('Expected sendResponseData() to stop after RST_STREAM');
        } catch (H2Exception $exception) {
            $this->assertSame('Cannot send on locally closed stream', $exception->getMessage());
        }

        $this->assertCount(1, $recordingConnection->frames);
        $this->assertSame('ab', $recordingConnection->frames[0]->payload);

        $streamRequest = $connection->tryRecvStreamRequest();
        $this->assertNotNull($streamRequest);
        $this->assertSame(3, $streamRequest['streamId']);
        $this->assertSame('/next', $streamRequest['request']->getUri()->getPath());
    }

    public function testSendResponseHeadersDoNotConsumeCumulativeSendWindow(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 64;
                $this->streamSendWindows[1] = 64;
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
        $connection->sendResponseData(1, 'hello', true);

        $this->assertCount(2, $recordingConnection->frames);
        $consumed = strlen($recordingConnection->frames[1]->payload);
        $this->assertSame(64 - $consumed, $recordingConnection->exposeConnectionSendWindow());
        $this->assertSame(64 - $consumed, $recordingConnection->exposeStreamSendWindow(1));
    }

    public function testSendResponseDataHandlesMultipleSmallWindowUpdatesAcrossStreams(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 1;
                $this->streamSendWindows[1] = 1;
                $this->streamSendWindows[3] = 1;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x03/s3"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/s5"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 3, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 3, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 3, pack('N', 1)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'abc', true);

        $first = $connection->tryRecvStreamRequest();
        $connection->sendResponseData(3, 'xyz', true);
        $second = $connection->tryRecvStreamRequest();

        $this->assertSame(3, $first['streamId']);
        $this->assertSame('/s3', $first['request']->getUri()->getPath());
        $this->assertSame(5, $second['streamId']);
        $this->assertSame('/s5', $second['request']->getUri()->getPath());
        $this->assertSame(
            ['a', 'b', 'c', 'x', 'y', 'z'],
            array_map(static fn(H2Frame $frame): string => $frame->payload, $recordingConnection->frames)
        );
    }

    public function testSendResponseDataHandlesLargeBodyAcrossManyWindowUpdates(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = 4;
                $this->streamSendWindows[1] = 4;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        foreach (range(1, 8) as $_) {
            $recordingConnection->readFrames[] = new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 4));
            $recordingConnection->readFrames[] = new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 4));
        }

        $payload = str_repeat('x', 36);
        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, $payload, true);

        $this->assertSame($payload, implode('', array_map(static fn(H2Frame $frame): string => $frame->payload, $recordingConnection->frames)));
        $this->assertSame(H2Frame::FLAG_END_STREAM, $recordingConnection->frames[array_key_last($recordingConnection->frames)]->flags);
        $this->assertGreaterThan(8, count($recordingConnection->frames));
    }

    public function testHalfClosedLocalAllowsWindowUpdateAndGoAwayButRstStreamCloses(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }

            public function exposeStreamSendWindow(int $streamId): int
            {
                return $this->streamSendWindows[$streamId] ?? self::DEFAULT_INITIAL_WINDOW_SIZE;
            }
        };
        $recordingConnection->markRequestStreamProcessed(1);

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 204, [], true);

        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 1024)));
        $recordingConnection->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)));

        $this->assertSame(H2Connection::STREAM_STATE_HALF_CLOSED_LOCAL, $recordingConnection->getStreamState(1));
        $this->assertSame(H2Connection::DEFAULT_INITIAL_WINDOW_SIZE + 1024, $recordingConnection->exposeStreamSendWindow(1));

        $recordingConnection->validateFrame(H2Frame::createRstStream(1, H2Exception::ERROR_CANCEL));

        $this->assertTrue($recordingConnection->isStreamClosed(1));
        $this->assertSame(H2Connection::STREAM_STATE_CLOSED, $recordingConnection->getStreamState(1));
    }

    public function testSendResponseDataPreservesFiveQueuedRequestFramesInOrder(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x03/a1"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/a2"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 7, "\x82\x87\x41\x0bexample.com\x44\x03/a3"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 9, "\x82\x87\x41\x0bexample.com\x44\x03/a4"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 11, "\x82\x87\x41\x0bexample.com\x44\x03/a5"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);

        $streamIds = [];
        foreach (range(1, 5) as $_) {
            $streamIds[] = $connection->tryRecvStreamRequest()['streamId'];
        }

        $this->assertSame([3, 5, 7, 9, 11], $streamIds);
    }

    public function testPendingGoAwayAllowsQueuedExistingStreamButRejectsQueuedNewStream(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x03/s3"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/s5"),
            new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 3, H2Exception::ERROR_NO_ERROR)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);

        $allowed = $connection->tryRecvStreamRequest();
        $this->assertSame(3, $allowed['streamId']);
        $this->assertSame('/s3', $allowed['request']->getUri()->getPath());
        $this->assertNull($connection->tryRecvStreamRequest());
    }

    public function testSendResponseDataPreservesNineQueuedRequestFramesInOrder(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x03/r1"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/r2"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 7, "\x82\x87\x41\x0bexample.com\x44\x03/r3"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 9, "\x82\x87\x41\x0bexample.com\x44\x03/r4"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 11, "\x82\x87\x41\x0bexample.com\x44\x03/r5"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 13, "\x82\x87\x41\x0bexample.com\x44\x03/r6"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 15, "\x82\x87\x41\x0bexample.com\x44\x03/r7"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 17, "\x82\x87\x41\x0bexample.com\x44\x03/r8"),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 19, "\x82\x87\x41\x0bexample.com\x44\x03/r9"),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);

        $streamIds = [];
        foreach (range(1, 9) as $_) {
            $streamIds[] = $connection->tryRecvStreamRequest()['streamId'];
        }

        $this->assertSame([3, 5, 7, 9, 11, 13, 15, 17, 19], $streamIds);
    }

    public function testPendingGoAwayPrefersQueuedOldStreamFramesOverQueuedNewStreamStarts(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[1] = 0;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->markRequestStreamProcessed(3);
        $recordingConnection->readFrames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 3, 'tail'),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 5, "\x82\x87\x41\x0bexample.com\x44\x03/n5"),
            new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 3, H2Exception::ERROR_NO_ERROR)),
            new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 2)),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);

        $body = $recordingConnection->readRequestBody(3);
        $this->assertSame('tail', $body);
        $this->assertNull($connection->tryRecvStreamRequest());
    }

    public function testHandleOneRequestSupportsRequestTrailerAndResponseTrailerOnSameStream(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->readFrames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->readFrames = [
            new H2Frame(
                H2Frame::TYPE_HEADERS,
                H2Frame::FLAG_END_HEADERS,
                1,
                "\x82\x87\x41\x0bexample.com\x44\x05/echo"
            ),
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'hello'),
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x40\x0ax-trailing\x03yes"),
        ];

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $streamId = $connection->handleOneRequest(
            static function ($request, int $streamId): H2ResponseEnvelope {
                return H2ResponseEnvelope::from(
                    response: Psr7::createResponse(200, body: 'ok'),
                    trailers: ['x-server-trailer' => 'done']
                );
            }
        );

        $this->assertSame(1, $streamId);
        $this->assertCount(3, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->frames[1]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[2]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $recordingConnection->frames[2]->flags);
    }

    public function testSendResponseStillWorksForExistingStreamAfterGoAway(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->markRequestStreamProcessed(1);
        $recordingConnection->goAway(lastStreamId: 1);

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-a' => 'b']);
        $connection->sendResponseData(1, 'ok', true);

        $this->assertCount(3, $recordingConnection->frames);
        $this->assertSame(H2Frame::TYPE_GOAWAY, $recordingConnection->frames[0]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $recordingConnection->frames[1]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $recordingConnection->frames[2]->type);
    }

    public function testSendResponseRejectsNewStreamAfterGoAway(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->markRequestStreamProcessed(1);
        $recordingConnection->goAway(lastStreamId: 1);

        $connection = new H2ServerConnection(connection: $recordingConnection);

        $this->expectException(\Swow\Http\Protocol\H2Exception::class);
        $this->expectExceptionMessage('New stream is not allowed after GOAWAY');

        $connection->sendResponseData(3, 'x', true);
    }

    public function testSendResponseHeadersEndStreamMarksHalfClosedLocal(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->markRequestStreamProcessed(1);

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-a' => 'b'], true);

        $this->assertSame(H2Connection::STREAM_STATE_HALF_CLOSED_LOCAL, $recordingConnection->getStreamState(1));
    }

    public function testSendResponseDataEndStreamClosesStreamAfterRemoteEnd(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
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

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82"),
        ];
        $recordingConnection->readHeaderBlock();

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseData(1, 'ok', true);

        $this->assertSame(H2Connection::STREAM_STATE_CLOSED, $recordingConnection->getStreamState(1));
        $this->assertTrue($recordingConnection->isStreamClosed(1));
    }

    public function testSendResponseDataRejectsAfterLocalSideIsClosed(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->markRequestStreamProcessed(1);

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-a' => 'b'], true);

        $this->expectException(\Swow\Http\Protocol\H2Exception::class);
        $this->expectExceptionMessage('Cannot send on locally closed stream');

        $connection->sendResponseData(1, 'late', true);
    }

    public function testSendResponseHeadersRejectsAfterLocalSideIsClosed(): void
    {
        $recordingConnection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };
        $recordingConnection->markRequestStreamProcessed(1);

        $connection = new H2ServerConnection(connection: $recordingConnection);
        $connection->sendResponseHeaders(1, 200, ['x-a' => 'b'], true);

        $this->expectException(\Swow\Http\Protocol\H2Exception::class);
        $this->expectExceptionMessage('Cannot send on locally closed stream');

        $connection->sendResponseHeaders(1, 200, ['x-b' => 'c']);
    }
}
