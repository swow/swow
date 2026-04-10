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

use Closure;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use RuntimeException;
use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\EventDriver;
use Swow\Psr7\Server\EventDriverConnectionHandler;
use Swow\Psr7\Server\H2ResponseEnvelope;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\Socket;

/**
 * @internal
 */
#[CoversClass(EventDriver::class)]
#[CoversClass(EventDriverConnectionHandler::class)]
final class EventDriverTest extends TestCase
{
    public function testEventDriverHandlesH2Connection(): void
    {
        $protocol = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $readFrames = [];
            /** @var list<H2Frame> */
            public array $writtenFrames = [];
            public int $prefaceReads = 0;
            public bool $initialized = false;

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
                    throw new RuntimeException('No frame queued');
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
        $driver = $this->createEventDriverRunner(
            static function ($connection, ServerRequestInterface $request): array {
                return [201, ['x-event-driver' => 'h2'], 'done'];
            }
        );

        $driver->runAcceptedConnection($connection);

        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertTrue($protocol->initialized);
        $this->assertCount(2, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
    }

    public function testEventDriverNaturallyStopsOnH2Eof(): void
    {
        $protocol = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public int $prefaceReads = 0;
            public bool $initialized = false;

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
                throw new RuntimeException('No frame queued');
            }
        };
        $connection = new H2ServerConnection(connection: $protocol);
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): ResponseInterface => Psr7::createResponse(204)
        );

        $driver->runAcceptedConnection($connection);

        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertTrue($protocol->initialized);
    }

    public function testEventDriverStillHandlesHttpConnection(): void
    {
        $request = Psr7::createServerRequest('GET', '/event-driver', []);
        $connection = new class(new Server(), $request) extends ServerConnection {
            public ?ResponseInterface $sentResponse = null;
            public bool $closed = false;
            public function __construct(Server $server, private ServerRequestInterface $request)
            {
                parent::__construct($server);
            }

            public function recvHttpRequest(?int $timeout = null): ServerRequestInterface
            {
                return $this->request;
            }

            public function sendHttpResponse(ResponseInterface $response): static
            {
                $this->sentResponse = $response;
                return $this;
            }

            public function shouldKeepAlive(): bool
            {
                return false;
            }

            public function close(): bool
            {
                $this->closed = true;
                return true;
            }
        };
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): ResponseInterface => Psr7::createResponse(202)
        );

        $driver->runAcceptedConnection($connection);

        $this->assertInstanceOf(ResponseInterface::class, $connection->sentResponse);
        $this->assertSame(202, $connection->sentResponse->getStatusCode());
        $this->assertTrue($connection->closed);
    }

    public function testEventDriverInvokesExceptionAndCloseHandlersForH2ConnectionError(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $caughtException = null;
        $closedConnection = null;
        $driver = $this->createEventDriverRunner(
            static function ($connection, ServerRequestInterface $request) {
                throw H2Exception::forConnectionError('boom');
            }
        )
            ->withExceptionHandler(function ($connection, \Throwable $exception) use (&$caughtException): void {
                $caughtException = $exception;
            })
            ->withCloseHandler(function ($connection) use (&$closedConnection): void {
                $closedConnection = $connection;
            });

        $driver->runAcceptedConnection($connection);

        $this->assertInstanceOf(H2Exception::class, $caughtException);
        $this->assertSame('boom', $caughtException->getMessage());
        $this->assertSame($connection, $closedConnection);
    }

    public function testEventDriverInvokesExceptionAndCloseHandlersForH2TypeError(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $caughtException = null;
        $closeCount = 0;
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): object => new \stdClass()
        )
            ->withExceptionHandler(function ($connection, \Throwable $exception) use (&$caughtException): void {
                $caughtException = $exception;
            })
            ->withCloseHandler(function () use (&$closeCount): void {
                $closeCount++;
            });

        $driver->runAcceptedConnection($connection);

        $this->assertInstanceOf(\TypeError::class, $caughtException);
        $this->assertSame(1, $closeCount);
    }

    public function testEventDriverInvokesExceptionHandlerForH2NullResponse(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $caughtException = null;
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request) => null
        )->withExceptionHandler(function ($connection, \Throwable $exception) use (&$caughtException): void {
            $caughtException = $exception;
        });

        $driver->runAcceptedConnection($connection);

        $this->assertInstanceOf(\TypeError::class, $caughtException);
    }

    public function testEventDriverH2AcceptsStatusOnlyResponse(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): int => 204
        );

        $driver->runAcceptedConnection($connection);

        $this->assertCount(1, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $protocol->writtenFrames[0]->flags);
    }

    public function testEventDriverH2AcceptsStringResponse(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): string => 'plain-body'
        );

        $driver->runAcceptedConnection($connection);

        $this->assertCount(2, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame('plain-body', $protocol->writtenFrames[1]->payload);
    }

    public function testEventDriverH2AcceptsArrayResponse(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): array => [203, ['x-array' => 'yes'], 'array-body']
        );

        $driver->runAcceptedConnection($connection);

        $this->assertCount(2, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame('array-body', $protocol->writtenFrames[1]->payload);
    }

    public function testEventDriverH2AcceptsResponseEnvelopeWithTrailers(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request) => H2ResponseEnvelope::from(
                Psr7::createResponse(200, body: 'array-body'),
                ['x-trailer' => 'done']
            )
        );

        $driver->runAcceptedConnection($connection);

        $this->assertCount(3, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[2]->type);
        $this->assertSame(H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, $protocol->writtenFrames[2]->flags);
    }

    public function testEventDriverH2AcceptsArrayResponseWithTrailers(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): array => [202, ['x-array' => 'yes'], 'array-body', ['x-trailer' => 'done']]
        );

        $driver->runAcceptedConnection($connection);

        $this->assertCount(3, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[2]->type);
    }

    public function testEventDriverH2AcceptsNamedResponseMapWithTrailers(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $connection = new H2ServerConnection(connection: $protocol);
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): array => [
                'status' => 202,
                'headers' => ['x-named' => 'yes'],
                'body' => 'array-body',
                'trailers' => ['x-trailer' => 'done'],
            ]
        );

        $driver->runAcceptedConnection($connection);

        $this->assertCount(3, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[1]->type);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[2]->type);
    }

    public function testEventDriverAutoPromotesDirectH2Preface(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $httpConnection = new class(new Server()) extends ServerConnection {
            public bool $closed = false;

            public function peekString(int $size = 8192, ?int $timeout = 0): string
            {
                return H2Connection::CLIENT_PREFACE;
            }

            public function close(): bool
            {
                $this->closed = true;
                return true;
            }
        };
        $driver = $this->createEventDriverRunnerWithOverrides(
            static fn($connection, ServerRequestInterface $request): string => 'promoted',
            static function (?Closure $connectionHandler, ?Closure $requestHandler, ?Closure $upgradeHandler, ?Closure $messageHandler, ?Closure $closeHandler, ?Closure $exceptionHandler) use ($protocol): EventDriverConnectionHandler {
                return new class($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler, $protocol) extends EventDriverConnectionHandler {
                    public function __construct(
                        ?Closure $connectionHandler,
                        ?Closure $requestHandler,
                        ?Closure $upgradeHandler,
                        ?Closure $messageHandler,
                        ?Closure $closeHandler,
                        ?Closure $exceptionHandler,
                        private H2Connection $protocol,
                    ) {
                        parent::__construct($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler);
                    }

                    protected function createH2ServerConnectionFromSocket(Socket $socket, array $serverParams = []): H2ServerConnection
                    {
                        return new H2ServerConnection(connection: $this->protocol);
                    }
                };
            }
        );

        $driver->runAcceptedConnection($httpConnection);

        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertTrue($protocol->initialized);
        $this->assertCount(2, $protocol->writtenFrames);
    }

    public function testEventDriverAutoPromotesNegotiatedH2AlpnWithoutPrefacePeek(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $httpConnection = new class(new Server()) extends ServerConnection {
            public bool $peeked = false;

            public function getNegotiatedAlpnProtocol(): ?string
            {
                return 'h2';
            }

            public function peekString(int $size = 8192, ?int $timeout = 0): string
            {
                $this->peeked = true;
                return 'GET / HTTP/1.1';
            }
        };
        $driver = $this->createEventDriverRunnerWithOverrides(
            static fn($connection, ServerRequestInterface $request): string => 'alpn-h2',
            static function (?Closure $connectionHandler, ?Closure $requestHandler, ?Closure $upgradeHandler, ?Closure $messageHandler, ?Closure $closeHandler, ?Closure $exceptionHandler) use ($protocol): EventDriverConnectionHandler {
                return new class($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler, $protocol) extends EventDriverConnectionHandler {
                    public function __construct(
                        ?Closure $connectionHandler,
                        ?Closure $requestHandler,
                        ?Closure $upgradeHandler,
                        ?Closure $messageHandler,
                        ?Closure $closeHandler,
                        ?Closure $exceptionHandler,
                        private H2Connection $protocol,
                    ) {
                        parent::__construct($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler);
                    }

                    protected function createH2ServerConnectionFromSocket(Socket $socket, array $serverParams = []): H2ServerConnection
                    {
                        return new H2ServerConnection(connection: $this->protocol);
                    }
                };
            }
        );

        $driver->runAcceptedConnection($httpConnection);

        $this->assertFalse($httpConnection->peeked);
        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertTrue($protocol->initialized);
        $this->assertCount(2, $protocol->writtenFrames);
    }

    public function testEventDriverKeepsHttpWhenNegotiatedAlpnIsHttp11(): void
    {
        $request = Psr7::createServerRequest('GET', '/event-driver', []);
        $connection = new class(new Server(), $request) extends ServerConnection {
            public ?ResponseInterface $sentResponse = null;
            public bool $peeked = false;

            public function __construct(Server $server, private ServerRequestInterface $request)
            {
                parent::__construct($server);
            }

            public function getNegotiatedAlpnProtocol(): ?string
            {
                return 'http/1.1';
            }

            public function peekString(int $size = 8192, ?int $timeout = 0): string
            {
                $this->peeked = true;
                return H2Connection::CLIENT_PREFACE;
            }

            public function recvHttpRequest(?int $timeout = null): ServerRequestInterface
            {
                return $this->request;
            }

            public function sendHttpResponse(ResponseInterface $response): static
            {
                $this->sentResponse = $response;
                return $this;
            }

            public function shouldKeepAlive(): bool
            {
                return false;
            }
        };
        $driver = $this->createEventDriverRunner(
            static fn($connection, ServerRequestInterface $request): ResponseInterface => Psr7::createResponse(202)
        );

        $driver->runAcceptedConnection($connection);

        $this->assertFalse($connection->peeked);
        $this->assertInstanceOf(ResponseInterface::class, $connection->sentResponse);
        $this->assertSame(202, $connection->sentResponse->getStatusCode());
    }

    public function testEventDriverAutoPromotesPartialDirectH2Preface(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $httpConnection = new class(new Server()) extends ServerConnection {
            public bool $closed = false;

            public function peekString(int $size = 8192, ?int $timeout = 0): string
            {
                return substr(H2Connection::CLIENT_PREFACE, 0, 8);
            }

            public function close(): bool
            {
                $this->closed = true;
                return true;
            }
        };
        $driver = $this->createEventDriverRunnerWithOverrides(
            static fn($connection, ServerRequestInterface $request): string => 'promoted-partial',
            static function (?Closure $connectionHandler, ?Closure $requestHandler, ?Closure $upgradeHandler, ?Closure $messageHandler, ?Closure $closeHandler, ?Closure $exceptionHandler) use ($protocol): EventDriverConnectionHandler {
                return new class($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler, $protocol) extends EventDriverConnectionHandler {
                    public function __construct(
                        ?Closure $connectionHandler,
                        ?Closure $requestHandler,
                        ?Closure $upgradeHandler,
                        ?Closure $messageHandler,
                        ?Closure $closeHandler,
                        ?Closure $exceptionHandler,
                        private H2Connection $protocol,
                    ) {
                        parent::__construct($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler);
                    }

                    protected function createH2ServerConnectionFromSocket(Socket $socket, array $serverParams = []): H2ServerConnection
                    {
                        return new H2ServerConnection(connection: $this->protocol);
                    }
                };
            }
        );

        $driver->runAcceptedConnection($httpConnection);

        $this->assertSame(1, $protocol->prefaceReads);
        $this->assertTrue($protocol->initialized);
        $this->assertCount(2, $protocol->writtenFrames);
    }

    public function testEventDriverHandlesH2cUpgrade(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([]);
        $request = Psr7::createServerRequest(
            'GET',
            '/h2c',
            [],
            [
                'Connection' => 'Upgrade, HTTP2-Settings',
                'Upgrade' => 'h2c',
                'HTTP2-Settings' => 'AAMAAABk',
            ]
        );
        $request->setIsUpgrade(true);
        $httpConnection = new class(new Server(), $request) extends ServerConnection {
            public bool $closed = false;
            public ?array $switchHeaders = null;

            public function __construct(Server $server, private ServerRequestInterface $request)
            {
                parent::__construct($server);
            }

            public function peekString(int $size = 8192, ?int $timeout = 0): string
            {
                return 'GET /h2c HTTP/1.1';
            }

            public function recvHttpRequest(?int $timeout = null): ServerRequestInterface
            {
                return $this->request;
            }

            public function sendHttpHeader(int $statusCode = 200, string $reasonPhrase = '', array $headers = [], string $protocolVersion = '1.1'): static
            {
                $this->switchHeaders = ['status' => $statusCode, 'headers' => $headers];
                return $this;
            }

            public function close(): bool
            {
                $this->closed = true;
                return true;
            }
        };
        $driver = $this->createEventDriverRunnerWithOverrides(
            static fn($connection, ServerRequestInterface $request): string => 'upgraded',
            static function (?Closure $connectionHandler, ?Closure $requestHandler, ?Closure $upgradeHandler, ?Closure $messageHandler, ?Closure $closeHandler, ?Closure $exceptionHandler) use ($protocol): EventDriverConnectionHandler {
                return new class($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler, $protocol) extends EventDriverConnectionHandler {
                    public function __construct(
                        ?Closure $connectionHandler,
                        ?Closure $requestHandler,
                        ?Closure $upgradeHandler,
                        ?Closure $messageHandler,
                        ?Closure $closeHandler,
                        ?Closure $exceptionHandler,
                        private H2Connection $protocol,
                    ) {
                        parent::__construct($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler);
                    }

                    protected function createH2ServerConnectionFromSocket(Socket $socket, array $serverParams = []): H2ServerConnection
                    {
                        return new H2ServerConnection(connection: $this->protocol);
                    }
                };
            }
        );

        $driver->runAcceptedConnection($httpConnection);

        $this->assertSame(101, $httpConnection->switchHeaders['status']);
        $this->assertSame('h2c', $httpConnection->switchHeaders['headers']['Upgrade']);
        $this->assertTrue($protocol->isInitialized());
        $this->assertCount(4, $protocol->writtenFrames);
        $this->assertSame(H2Frame::TYPE_SETTINGS, $protocol->writtenFrames[0]->type);
        $this->assertSame(H2Frame::TYPE_SETTINGS, $protocol->writtenFrames[1]->type);
        $this->assertSame(H2Frame::FLAG_ACK, $protocol->writtenFrames[1]->flags);
        $this->assertSame(H2Frame::TYPE_HEADERS, $protocol->writtenFrames[2]->type);
        $this->assertSame(H2Frame::TYPE_DATA, $protocol->writtenFrames[3]->type);
    }

    public function testEventDriverHandlesH2cUpgradeRequestBodyAndSubsequentStream(): void
    {
        $protocol = $this->createH2ProtocolWithFrames([
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 3, "\x82\x87\x41\x0bexample.com\x44\x01/"),
        ]);
        $request = Psr7::createServerRequest(
            'POST',
            '/h2c',
            [],
            [
                'Connection' => 'Upgrade, HTTP2-Settings',
                'Upgrade' => 'h2c',
                'HTTP2-Settings' => 'AAMAAABk',
            ],
            'upgrade-body',
        );
        $request->setIsUpgrade(true);
        $httpConnection = new class(new Server(), $request) extends ServerConnection {
            public ?array $switchHeaders = null;

            public function __construct(Server $server, private ServerRequestInterface $request)
            {
                parent::__construct($server);
            }

            public function peekString(int $size = 8192, ?int $timeout = 0): string
            {
                return 'POST /h2c HTTP/1.1';
            }

            public function recvHttpRequest(?int $timeout = null): ServerRequestInterface
            {
                return $this->request;
            }

            public function sendHttpHeader(int $statusCode = 200, string $reasonPhrase = '', array $headers = [], string $protocolVersion = '1.1'): static
            {
                $this->switchHeaders = ['status' => $statusCode, 'headers' => $headers];
                return $this;
            }
        };
        $requests = [];
        $driver = $this->createEventDriverRunnerWithOverrides(
            static function ($connection, ServerRequestInterface $request) use (&$requests): string {
                $requests[] = [
                    'method' => $request->getMethod(),
                    'path' => $request->getUri()->getPath(),
                    'body' => (string) $request->getBody(),
                ];
                return 'ok';
            },
            static function (?Closure $connectionHandler, ?Closure $requestHandler, ?Closure $upgradeHandler, ?Closure $messageHandler, ?Closure $closeHandler, ?Closure $exceptionHandler) use ($protocol): EventDriverConnectionHandler {
                return new class($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler, $protocol) extends EventDriverConnectionHandler {
                    public function __construct(
                        ?Closure $connectionHandler,
                        ?Closure $requestHandler,
                        ?Closure $upgradeHandler,
                        ?Closure $messageHandler,
                        ?Closure $closeHandler,
                        ?Closure $exceptionHandler,
                        private H2Connection $protocol,
                    ) {
                        parent::__construct($connectionHandler, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler);
                    }

                    protected function createH2ServerConnectionFromSocket(Socket $socket, array $serverParams = []): H2ServerConnection
                    {
                        return new H2ServerConnection(connection: $this->protocol);
                    }
                };
            }
        );

        $driver->runAcceptedConnection($httpConnection);

        $this->assertSame(101, $httpConnection->switchHeaders['status']);
        $this->assertSame(
            [
                ['method' => 'POST', 'path' => '/h2c', 'body' => 'upgrade-body'],
                ['method' => 'GET', 'path' => '/', 'body' => ''],
            ],
            $requests
        );
        $this->assertCount(6, $protocol->writtenFrames);
    }

    public function testEventDriverRejectsInvalidH2cSettingsPayload(): void
    {
        $request = Psr7::createServerRequest(
            'GET',
            '/h2c',
            [],
            [
                'Connection' => 'Upgrade, HTTP2-Settings',
                'Upgrade' => 'h2c',
                'HTTP2-Settings' => 'invalid',
            ]
        );
        $request->setIsUpgrade(true);
        $httpConnection = new class(new Server(), $request) extends ServerConnection {
            public ?array $errorCall = null;
            public bool $closed = false;

            public function __construct(Server $server, private ServerRequestInterface $request)
            {
                parent::__construct($server);
            }

            public function peekString(int $size = 8192, ?int $timeout = 0): string
            {
                return 'GET /h2c HTTP/1.1';
            }

            public function recvHttpRequest(?int $timeout = null): ServerRequestInterface
            {
                return $this->request;
            }

            public function error(int $statusCode, string $message = '', ?bool $close = null): void
            {
                $this->errorCall = ['status' => $statusCode, 'message' => $message, 'close' => $close];
            }

            public function close(): bool
            {
                $this->closed = true;
                return true;
            }
        };
        $driver = $this->createEventDriverRunner(
            static function () {
                throw new RuntimeException('request handler should not be called');
            }
        );

        $driver->runAcceptedConnection($httpConnection);

        $this->assertSame(400, $httpConnection->errorCall['status']);
        $this->assertSame(true, $httpConnection->errorCall['close']);
        $this->assertTrue($httpConnection->closed);
    }

    private function createEventDriverRunner(callable $requestHandler): EventDriver
    {
        return $this->createEventDriverRunnerWithOverrides($requestHandler, null);
    }

    private function createEventDriverRunnerWithOverrides(callable $requestHandler, ?callable $factory): EventDriver
    {
        $driver = (new class extends EventDriver {
            public $factory = null;

            public function runAcceptedConnection(ServerConnection|H2ServerConnection $connection): void
            {
                $this->createRuntimeHandler()->handle($connection);
            }

            private function createRuntimeHandler(): EventDriverConnectionHandler
            {
                if ($this->factory !== null) {
                    return ($this->factory)(
                        isset($this->connectionHandler) ? $this->connectionHandler : null,
                        isset($this->requestHandler) ? $this->requestHandler : null,
                        isset($this->upgradeHandler) ? $this->upgradeHandler : null,
                        isset($this->messageHandler) ? $this->messageHandler : null,
                        isset($this->closeHandler) ? $this->closeHandler : null,
                        isset($this->exceptionHandler) ? $this->exceptionHandler : null,
                    );
                }

                return $this->createConnectionHandler(
                    isset($this->connectionHandler) ? $this->connectionHandler : null,
                    isset($this->requestHandler) ? $this->requestHandler : null,
                    isset($this->upgradeHandler) ? $this->upgradeHandler : null,
                    isset($this->messageHandler) ? $this->messageHandler : null,
                    isset($this->closeHandler) ? $this->closeHandler : null,
                    isset($this->exceptionHandler) ? $this->exceptionHandler : null,
                );
            }
        })->withRequestHandler($requestHandler);
        $driver->factory = $factory;

        return $driver;
    }

    /**
     * @param list<H2Frame> $frames
     */
    private function createH2ProtocolWithFrames(array $frames): H2Connection
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
                    throw new RuntimeException('No frame queued');
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
