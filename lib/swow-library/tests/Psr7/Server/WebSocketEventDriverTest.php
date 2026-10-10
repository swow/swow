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
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Channel;
use Swow\Coroutine;
use Swow\Http\Http;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\WebSocketFrame;
use Swow\Psr7\Message\WebSocketFrameInterface;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\EventDriver;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\WebSocket\Opcode;

/** @internal */
#[CoversClass(EventDriver::class)]
#[CoversClass(ServerConnection::class)]
#[CoversClass(Client::class)]
#[CoversClass(WebSocketFrame::class)]
#[CoversClass(Psr7::class)]
final class WebSocketEventDriverTest extends TestCase
{
    #[DataProvider('provideUpgradeControlFramesAndApplicationRepliesCases')]
    public function testUpgradeControlFramesAndApplicationReplies(bool $frameReply): void
    {
        $server = new Server();
        $server->setRecvMessageTimeout(1000);
        $ready = new Channel(1);
        $closed = new Channel(1);
        $messages = [];
        $driver = (new EventDriver($server))
            ->withStartHandler(static function (Server $started) use ($ready): void {
                $ready->push($started->getSockPort());
            })
            ->withRequestHandler(static fn(): string => 'ordinary response')
            ->withMessageHandler(static function (ServerConnection $connection, WebSocketFrameInterface $frame) use ($frameReply, &$messages): string|WebSocketFrame {
                $payload = (string) $frame->getPayloadData();
                $messages[] = [$frame->getOpcode(), $payload];
                return $frameReply ? Psr7::createWebSocketBinaryFrame($payload) : $payload;
            })
            ->withCloseHandler(static function (ServerConnection $connection) use ($closed): void {
                $closed->push($connection);
            });
        $loop = Coroutine::run(static function () use ($driver): void {
            $driver->startOn('127.0.0.1', 0);
        });
        $client = new Client();
        try {
            $client->setTimeout(1000)->connect('127.0.0.1', $ready->pop(1000));
            $response = $client->upgradeToWebSocket(Psr7::createRequest('GET', 'http://example.test/events'));
            $this->assertSame(Status::SWITCHING_PROTOCOLS, $response->getStatusCode());
            $this->assertSame('websocket', $response->getHeaderLine('Upgrade'));
            $client->sendWebSocketFrame(Psr7::createWebSocketFrame(Opcode::PING, '', mask: true));
            $pong = $client->recvWebSocketFrame();
            $this->assertSame(Opcode::PONG, $pong->getOpcode());
            $this->assertSame('', (string) $pong->getPayloadData());
            $client->sendWebSocketFrame(Psr7::createWebSocketFrame(Opcode::PONG, '', mask: true));
            $client->sendWebSocketFrame(Psr7::createWebSocketTextMaskedFrame("message\0data"));
            $reply = $client->recvWebSocketFrame();
            $this->assertSame($frameReply ? Opcode::BINARY : Opcode::TEXT, $reply->getOpcode());
            $this->assertSame("message\0data", (string) $reply->getPayloadData());
            $this->assertSame('', $reply->getMaskingKey());
            $this->assertSame([[Opcode::TEXT, "message\0data"]], $messages);
            $client->sendWebSocketFrame(Psr7::createWebSocketFrame(Opcode::CLOSE, '', mask: true));
            $connection = $closed->pop(1000);
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

    /** @return iterable<string, array{bool}> */
    public static function provideUpgradeControlFramesAndApplicationRepliesCases(): iterable
    {
        yield 'string reply' => [false];
        yield 'frame reply' => [true];
    }

    #[DataProvider('provideInvalidUpgradeIsRejectedAndClosedCases')]
    public function testInvalidUpgradeIsRejectedAndClosed(string $upgrade, string $reason): void
    {
        $server = new Server();
        $server->setRecvMessageTimeout(1000);
        $ready = new Channel(1);
        $closed = new Channel(1);
        $requests = 0;
        $driver = (new EventDriver($server))
            ->withStartHandler(static function (Server $started) use ($ready): void {
                $ready->push($started->getSockPort());
            })
            ->withRequestHandler(static function () use (&$requests): string {
                $requests++;
                return 'ordinary response';
            })
            ->withMessageHandler(static fn(): string => 'reply')
            ->withCloseHandler(static function (ServerConnection $connection) use ($closed): void {
                $closed->push($connection);
            });
        $loop = Coroutine::run(static function () use ($driver): void {
            $driver->startOn('127.0.0.1', 0);
        });
        $client = new Client();
        try {
            $client->setTimeout(1000)->connect('127.0.0.1', $ready->pop(1000));
            $client->getSocket()->send(Http::packRequest('GET', '/upgrade', [
                'Host' => 'example.test',
                'Connection' => 'Upgrade',
                'Upgrade' => $upgrade,
                'Sec-WebSocket-Key' => 'invalid',
            ]));
            $response = Psr7::createResponseFromEntity($client->recvResponseEntity());
            $this->assertSame(Status::BAD_REQUEST, $response->getStatusCode());
            $this->assertStringContainsString($reason, (string) $response->getBody());
            $this->assertSame(0, $requests);
            $connection = $closed->pop(1000);
            $this->assertSame('', $client->getSocket()->recvString(1));
            $this->assertFalse($connection->getSocket()->isAvailable());
        } finally {
            $client->close();
            $server->closeConnections();
            $driver->stop();
            if ($loop->isAlive()) {
                $loop->kill();
            }
        }
    }

    /** @return iterable<string, array{string, string}> */
    public static function provideInvalidUpgradeIsRejectedAndClosedCases(): iterable
    {
        yield 'invalid websocket key' => ['websocket', 'Invalid Secret Key'];
        yield 'unsupported h2c upgrade' => ['h2c', 'Unsupported Upgrade Type'];
    }
}
