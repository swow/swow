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
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Exception;
use Swow\Coroutine;
use Swow\Http\Protocol\H2Frame;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Psr7\Server\H2ServerConnectionFactory;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

/**
 * @internal
 */
#[CoversClass(H2ServerConnectionFactory::class)]
#[CoversClass(H2ServerConnection::class)]
final class H2ServerFactoryTest extends TestCase
{
    public function testServerAcceptConnectionReturnsH2ConnectionWhenUsingH2Factory(): void
    {
        $server = new Server();
        $server->setServerConnectionFactory(new H2ServerConnectionFactory());
        try {
            $server->bind('127.0.0.1')->listen();
        } catch (SocketException $exception) {
            $this->markTestSkipped('Socket bind is not permitted in current environment: ' . $exception->getMessage());
        }

        $wr = new WaitReference();
        $acceptedConnection = null;
        Coroutine::run(static function () use ($server, &$acceptedConnection, $wr): void {
            $acceptedConnection = $server->acceptConnection();
        });

        $client = new Socket(Socket::TYPE_TCP);
        $client->connect($server->getSockAddress(), $server->getSockPort());

        $wr::wait($wr);

        $this->assertInstanceOf(H2ServerConnection::class, $acceptedConnection);
        $this->assertSame($client->getSockAddress(), $acceptedConnection->getServerParams()['remote_addr']);
        $this->assertSame($client->getSockPort(), $acceptedConnection->getServerParams()['remote_port']);

        $client->close();
        $acceptedConnection?->close();
        $server->close();
    }

    public function testServerAcceptConnectionStillReturnsHttpConnectionByDefault(): void
    {
        $server = new Server();
        try {
            $server->bind('127.0.0.1')->listen();
        } catch (SocketException $exception) {
            $this->markTestSkipped('Socket bind is not permitted in current environment: ' . $exception->getMessage());
        }

        $wr = new WaitReference();
        $acceptedConnection = null;
        Coroutine::run(static function () use ($server, &$acceptedConnection, $wr): void {
            $acceptedConnection = $server->acceptConnection();
        });

        $client = new Socket(Socket::TYPE_TCP);
        $client->connect($server->getSockAddress(), $server->getSockPort());

        $wr::wait($wr);

        $this->assertInstanceOf(ServerConnection::class, $acceptedConnection);

        $client->close();
        $acceptedConnection?->close();
        $server->close();
    }

    public function testH2ConnectionServeRunsPrefaceInitializeAndHandler(): void
    {
        $protocol = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];
            public int $prefaceReads = 0;
            /** @var array{timeout: ?int, settings: array<int,int>}|null */
            public ?array $initializeCall = null;

            public function readClientPreface(?int $timeout = null): void
            {
                $this->prefaceReads++;
            }

            public function initialize(?int $timeout = null, array $settings = []): void
            {
                $this->initializeCall = ['timeout' => $timeout, 'settings' => $settings];
            }

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
        $protocol->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];

        $connection = new H2ServerConnection(connection: $protocol);
        $handled = $connection->serve(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            ['settings' => [H2Connection::SETTINGS_MAX_FRAME_SIZE => 32768], 'limit' => 1]
        );

        $this->assertSame(1, $handled);
        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertSame(['timeout' => null, 'settings' => [H2Connection::SETTINGS_MAX_FRAME_SIZE => 32768]], $protocol->initializeCall);
        $this->assertCount(1, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
    }

    public function testServerHandleH2ConnectionUsesAcceptedH2Connection(): void
    {
        $protocol = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];
            public int $prefaceReads = 0;

            public function readClientPreface(?int $timeout = null): void
            {
                $this->prefaceReads++;
            }

            public function initialize(?int $timeout = null, array $settings = []): void
            {
            }

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
        $protocol->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];
        $acceptedConnection = new H2ServerConnection(connection: $protocol);

        $server = new class($acceptedConnection) extends Server {
            public function __construct(private H2ServerConnection $acceptedConnection)
            {
                parent::__construct();
            }

            public function acceptConnection(?int $timeout = null): ServerConnection|H2ServerConnection
            {
                return $this->acceptedConnection;
            }
        };

        $handled = $server->handleH2Connection(
            static fn($request, int $streamId) => Psr7::createResponse(204),
            ['limit' => 1]
        );

        $this->assertSame(1, $handled);
        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertCount(1, $protocol->writtenFrames);
    }

    public function testServerHandleH2ConnectionAcceptsNamedResponseMapWithTrailers(): void
    {
        $protocol = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];

            public function readClientPreface(?int $timeout = null): void
            {
            }

            public function initialize(?int $timeout = null, array $settings = []): void
            {
            }

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
        $protocol->readFrames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ];
        $acceptedConnection = new H2ServerConnection(connection: $protocol);

        $server = new class($acceptedConnection) extends Server {
            public function __construct(private H2ServerConnection $acceptedConnection)
            {
                parent::__construct();
            }

            public function acceptConnection(?int $timeout = null): ServerConnection|H2ServerConnection
            {
                return $this->acceptedConnection;
            }
        };

        $handled = $server->handleH2Connection(
            static fn($request, int $streamId): array => [
                'status' => 202,
                'headers' => ['x-server' => 'yes'],
                'body' => 'ok',
                'trailers' => ['x-trailer' => 'done'],
            ],
            ['limit' => 1]
        );

        $this->assertSame(1, $handled);
        $this->assertCount(3, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[2]->type);
    }

    public function testServerHandleH2ConnectionRejectsDefaultHttpConnection(): void
    {
        $server = new class extends Server {
            public function acceptConnection(?int $timeout = null): ServerConnection|H2ServerConnection
            {
                return new ServerConnection($this);
            }
        };

        $this->expectException(\RuntimeException::class);
        $this->expectExceptionMessage('Accepted connection is not an H2 server connection');

        $server->handleH2Connection(static fn($request, int $streamId) => Psr7::createResponse(204));
    }

    public function testServerHandleH2ConnectionAcceptsArrayResponseLikeEventDriver(): void
    {
        $protocol = $this->createProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $acceptedConnection = new H2ServerConnection(connection: $protocol);
        $server = $this->createAcceptedH2Server($acceptedConnection);

        $handled = $server->handleH2Connection(
            static fn(ServerRequestInterface $request, int $streamId): array => [203, ['x-handle' => 'server'], 'handled'],
            ['limit' => 1]
        );

        $this->assertSame(1, $handled);
        $this->assertCount(2, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame('handled', $protocol->writtenFrames[1]->payload);
    }

    public function testServerHandleH2ConnectionStopsNaturallyOnEofLikeEventDriver(): void
    {
        $protocol = $this->createProtocolWithFrames([]);
        $acceptedConnection = new H2ServerConnection(connection: $protocol);
        $server = $this->createAcceptedH2Server($acceptedConnection);

        $handled = $server->handleH2Connection(
            static fn(ServerRequestInterface $request, int $streamId): ResponseInterface => Psr7::createResponse(204)
        );

        $this->assertSame(0, $handled);
        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertTrue($protocol->initialized);
    }

    public function testServerHandleH2ConnectionPropagatesConnectionLevelErrorsLikeEventDriver(): void
    {
        $protocol = $this->createProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $acceptedConnection = new H2ServerConnection(connection: $protocol);
        $server = $this->createAcceptedH2Server($acceptedConnection);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('connection-failed');

        $server->handleH2Connection(
            static function (ServerRequestInterface $request, int $streamId) {
                throw H2Exception::forConnectionError('connection-failed');
            },
            ['limit' => 1]
        );
    }

    public function testServerHandleH2ConnectionRejectsNullResponse(): void
    {
        $protocol = $this->createProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $acceptedConnection = new H2ServerConnection(connection: $protocol);
        $server = $this->createAcceptedH2Server($acceptedConnection);

        $this->expectException(\TypeError::class);

        $server->handleH2Connection(
            static fn(ServerRequestInterface $request, int $streamId) => null,
            ['limit' => 1]
        );
    }

    public function testServerHandleH2ConnectionAcceptsStatusOnlyResponse(): void
    {
        $protocol = $this->createProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $acceptedConnection = new H2ServerConnection(connection: $protocol);
        $server = $this->createAcceptedH2Server($acceptedConnection);

        $handled = $server->handleH2Connection(
            static fn(ServerRequestInterface $request, int $streamId): int => 204,
            ['limit' => 1]
        );

        $this->assertSame(1, $handled);
        $this->assertCount(1, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $protocol->writtenFrames[0]->flags);
    }

    public function testServerHandleH2ConnectionAcceptsBodyOnlyResponse(): void
    {
        $protocol = $this->createProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $acceptedConnection = new H2ServerConnection(connection: $protocol);
        $server = $this->createAcceptedH2Server($acceptedConnection);

        $handled = $server->handleH2Connection(
            static fn(ServerRequestInterface $request, int $streamId): string => 'body-only',
            ['limit' => 1]
        );

        $this->assertSame(1, $handled);
        $this->assertCount(2, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame('body-only', $protocol->writtenFrames[1]->payload);
    }

    public function testServerHandleH2ConnectionRejectsUnsupportedResponseValue(): void
    {
        $protocol = $this->createProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $acceptedConnection = new H2ServerConnection(connection: $protocol);
        $server = $this->createAcceptedH2Server($acceptedConnection);

        $this->expectException(\TypeError::class);

        $server->handleH2Connection(
            static fn(ServerRequestInterface $request, int $streamId): object => new \stdClass(),
            ['limit' => 1]
        );
    }

    private function createAcceptedH2Server(H2ServerConnection $acceptedConnection): Server
    {
        return new class($acceptedConnection) extends Server {
            public function __construct(private H2ServerConnection $acceptedConnection)
            {
                parent::__construct();
            }

            public function acceptConnection(?int $timeout = null): ServerConnection|H2ServerConnection
            {
                return $this->acceptedConnection;
            }
        };
    }

    /**
     * @param list<H2Frame> $frames
     */
    private function createProtocolWithFrames(array $frames): H2Connection
    {
        return new class(new Socket(Socket::TYPE_TCP), $frames) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames;
            /** @var list<H2Frame> */
            public array $writtenFrames = [];
            public int $prefaceReads = 0;
            public bool $initialized = false;

            /**
             * @param list<H2Frame> $frames
             */
            public function __construct(Socket $socket, array $frames)
            {
                parent::__construct($socket);
                $this->readFrames = $frames;
            }

            public function readClientPreface(?int $timeout = null): void
            {
                $this->prefaceReads++;
            }

            public function initialize(?int $timeout = null, array $settings = []): void
            {
                $this->initialized = true;
            }

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
    }
}
