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

use Exception;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use RuntimeException;
use Swow\Channel;
use Swow\Coroutine;
use Swow\Http\Http;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ServerRequestPlusInterface;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\EventDriver;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;

/** @internal */
#[CoversClass(EventDriver::class)]
#[CoversClass(Server::class)]
#[CoversClass(ServerConnection::class)]
#[CoversClass(Client::class)]
#[CoversClass(Psr7::class)]
final class EventDriverTest extends TestCase
{
    #[DataProvider('provideRequestDispatchAndConnectionCleanupCases')]
    public function testRequestDispatchAndConnectionCleanup(string $kind, int $status, string $body): void
    {
        $server = new Server();
        $ready = new Channel(1);
        $closed = new Channel(1);
        $events = [];
        $driver = (new EventDriver($server))
            ->withWorkerCount(0)
            ->withStartHandler(static function (Server $started) use ($ready, &$events): void {
                $events[] = 'start';
                $ready->push($started->getSockPort());
            })
            ->withConnectionHandler(static function (ServerConnection $connection) use (&$events): void {
                $connection->getSocket()->setTimeout(1000);
                $events[] = 'connection';
            })
            ->withRequestHandler(static function (ServerConnection $connection, ServerRequestPlusInterface $request) use ($kind, &$events): mixed {
                $events[] = $request->getRequestTarget();
                return match ($kind) {
                    'text' => 'pong',
                    'arguments' => [Status::CREATED, ['X-Result' => 'created'], 'created'],
                    'psr' => Psr7::createResponse(Status::ACCEPTED, body: 'accepted'),
                };
            })
            ->withCloseHandler(static function (ServerConnection $connection) use ($closed, &$events): void {
                $events[] = 'close';
                $closed->push($connection);
            });
        $loop = Coroutine::run(static function () use ($driver): void {
            $driver->startOn('127.0.0.1', 0);
        });
        $client = new Client();
        try {
            $client->setTimeout(1000)->connect('127.0.0.1', $ready->pop(1000));
            $client->getSocket()->send(Http::packRequest('GET', '/request', ['Host' => 'example.test', 'Connection' => 'close']));
            $response = Psr7::createResponseFromEntity($client->recvResponseEntity());
            $this->assertSame($status, $response->getStatusCode());
            $this->assertSame($body, (string) $response->getBody());
            $this->assertSame('close', $response->getHeaderLine('Connection'));
            $connection = $closed->pop(1000);
            $this->assertSame(['start', 'connection', '/request', 'close'], $events);
            $this->assertSame('', $client->getSocket()->recvString(1));
            $this->assertFalse($connection->getSocket()->isAvailable());
            $this->assertSame([], iterator_to_array($server->getConnections()));
        } finally {
            $client->close();
            $server->closeConnections();
            $driver->stop();
            if ($loop->isAlive()) {
                $loop->kill();
            }
        }
    }

    /** @return iterable<string, array{string, int, string}> */
    public static function provideRequestDispatchAndConnectionCleanupCases(): iterable
    {
        yield 'text response' => ['text', Status::OK, 'pong'];
        yield 'response arguments' => ['arguments', Status::CREATED, 'created'];
        yield 'PSR response' => ['psr', Status::ACCEPTED, 'accepted'];
    }

    public function testInvokableExceptionHandlerRunsOnConfiguredCopy(): void
    {
        $server = new Server();
        $ready = new Channel(1);
        $closed = new Channel(1);
        $error = new RuntimeException('handler failed');
        $handler = new class {
            /** @var array<array{ServerConnection, Exception}> */
            public array $errors = [];

            public function __invoke(ServerConnection $connection, Exception $exception): void
            {
                $this->errors[] = [$connection, $exception];
            }
        };
        $original = (new EventDriver($server))
            ->withStartHandler(static function (Server $started) use ($ready): void {
                $ready->push($started->getSockPort());
            })
            ->withRequestHandler(static function () use ($error): never {
                throw $error;
            })
            ->withCloseHandler(static function (ServerConnection $connection) use ($closed): void {
                $closed->push($connection);
            });
        $configured = $original->withExceptionHandler($handler);
        $this->assertNotSame($original, $configured);
        $loop = Coroutine::run(static function () use ($configured): void {
            $configured->startOn('127.0.0.1', 0);
        });
        $client = new Client();
        try {
            $client->setTimeout(1000)->connect('127.0.0.1', $ready->pop(1000));
            $client->getSocket()->send(Http::packRequest('GET', '/failure', ['Host' => 'example.test']));
            $connection = $closed->pop(1000);
            $this->assertSame([[$connection, $error]], $handler->errors);
            // The close handler runs before close(); observe peer EOF before asserting cleanup.
            $this->assertSame('', $client->getSocket()->recvString(1));
            $this->assertFalse($connection->getSocket()->isAvailable());
            $this->assertSame([], iterator_to_array($server->getConnections()));
        } finally {
            $client->close();
            $server->closeConnections();
            $configured->stop();
            if ($loop->isAlive()) {
                $loop->kill();
            }
        }
    }
}
