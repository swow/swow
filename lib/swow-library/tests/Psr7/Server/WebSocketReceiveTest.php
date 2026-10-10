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
use Swow\Buffer;
use Swow\Coroutine;
use Swow\Http\Protocol\ProtocolException;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\WebSocketFrame;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\SocketException;
use Swow\WebSocket\Header;
use Swow\WebSocket\Opcode;
use Swow\WebSocket\WebSocket;

/** @internal */
#[CoversClass(Client::class)]
#[CoversClass(Server::class)]
#[CoversClass(ServerConnection::class)]
#[CoversClass(WebSocketFrame::class)]
#[CoversClass(Psr7::class)]
final class WebSocketReceiveTest extends TestCase
{
    private Server $server;

    private Client $client;

    private ServerConnection $connection;

    protected function setUp(): void
    {
        $this->server = new Server();
        $this->server->setTimeout(1000)->bind('127.0.0.1')->listen();
        $this->client = new Client();
        $this->client->setTimeout(1000)->connect('127.0.0.1', $this->server->getSockPort());
        $this->connection = $this->server->acceptConnection(1000);
    }

    protected function tearDown(): void
    {
        $this->connection->close();
        $this->client->close();
        $this->server->close();
    }

    #[DataProvider('provideSplitHeaderAndPayloadLeaveTheNextFrameIntactCases')]
    public function testSplitHeaderAndPayloadLeaveTheNextFrameIntact(int $length, bool $masked): void
    {
        $payload = substr(str_repeat("a\0\xffb", (int) ceil($length / 4)), 0, $length);
        $frame = Psr7::createWebSocketBinaryFrame($payload, mask: $masked ? 'mask' : false);
        $wire = $frame->toString();
        $headerSize = $frame->getHeaderSize();
        $writer = Coroutine::run(function () use ($wire, $headerSize): void {
            $socket = $this->client->getSocket();
            // Deliver the base header, extended header, and body in separate receive turns.
            $socket->send(substr($wire, 0, 1));
            usleep(1000);
            $socket->send(substr($wire, 1, $headerSize - 1));
            usleep(1000);
            $socket->send(substr($wire, $headerSize) . WebSocket::PING_FRAME);
        });
        try {
            $received = $this->connection->recvWebSocketFrame();
            $this->assertSame(Opcode::BINARY, $received->getOpcode());
            $this->assertTrue($received->getFin());
            $this->assertSame($payload, (string) $received->getPayloadData());
            $this->assertSame($length, $received->getPayloadLength());
            $this->assertFalse($received->getMask());
            $this->assertSame('', $received->getMaskingKey());
            $next = $this->connection->recvWebSocketFrame();
            $this->assertSame(Opcode::PING, $next->getOpcode());
            $this->assertSame('', (string) $next->getPayloadData());
            $this->assertSame($payload, (string) $received->getPayloadData());
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
        }
    }

    /** @return iterable<string, array{int, bool}> */
    public static function provideSplitHeaderAndPayloadLeaveTheNextFrameIntactCases(): iterable
    {
        foreach ([125, 126, Buffer::COMMON_SIZE + 32, 65536] as $length) {
            yield "unmasked {$length}" => [$length, false];
            yield "masked {$length}" => [$length, true];
        }
    }

    public function testManualUnmaskPreservesTheWirePayloadAndMaskingKey(): void
    {
        $payload = "binary\0\xffpayload";
        $frame = Psr7::createWebSocketBinaryFrame($payload, mask: 'mask');
        $this->assertTrue($this->connection->isAutoUnmask());
        $this->assertSame($this->connection, $this->connection->setAutoUnmask(false));
        $this->assertFalse($this->connection->isAutoUnmask());
        $this->client->sendWebSocketFrame($frame);
        $received = $this->connection->recvWebSocketFrameEntity();
        $this->assertTrue($received->getMask());
        $this->assertSame('mask', $received->getMaskingKey());
        $this->assertSame((string) $frame->getPayloadData(), (string) $received->payloadData);
        $this->assertNotNull($received->payloadData);
        WebSocket::unmask($received->payloadData, maskingKey: $received->getMaskingKey());
        $this->assertSame($payload, (string) $received->payloadData);
    }

    public function testContinuationFramesRetainTheirOwnBoundaries(): void
    {
        $first = Psr7::createWebSocketTextFrame('first', fin: false, mask: 'mask');
        $last = Psr7::createWebSocketFrame(Opcode::CONTINUATION, 'last', mask: 'mask');
        $this->client->getSocket()->send($first->toString() . WebSocket::PING_FRAME . $last->toString());
        $received = $this->connection->recvWebSocketFrame();
        $this->assertFalse($received->getFin());
        $this->assertSame(Opcode::TEXT, $received->getOpcode());
        $this->assertSame('first', (string) $received->getPayloadData());
        $this->assertSame(Opcode::PING, $this->connection->recvWebSocketFrame()->getOpcode());
        $received = $this->connection->recvWebSocketFrame();
        $this->assertTrue($received->getFin());
        $this->assertSame(Opcode::CONTINUATION, $received->getOpcode());
        $this->assertSame('last', (string) $received->getPayloadData());
        $this->connection->sendWebSocketPingFrame()->sendWebSocketPongFrame();
        $this->assertSame(Opcode::PING, $this->client->recvWebSocketFrame()->getOpcode());
        $this->assertSame(Opcode::PONG, $this->client->recvWebSocketFrame()->getOpcode());
    }

    public function testOversizedPayloadIsRejectedBeforeReadingItsBody(): void
    {
        $this->server->setMaxContentLength(4);
        $this->client->getSocket()->send((new Header(fin: true, opcode: Opcode::BINARY, payloadLength: 5))->toString());
        $this->expectException(ProtocolException::class);
        $this->expectExceptionCode(Status::REQUEST_ENTITY_TOO_LARGE);
        $this->connection->recvWebSocketFrame();
    }

    public function testTruncatedPayloadRaisesASocketException(): void
    {
        $this->client->getSocket()->send((new Header(fin: true, opcode: Opcode::BINARY, payloadLength: 5))->toString() . 'ab');
        $this->client->close();
        $this->expectException(SocketException::class);
        $this->connection->recvWebSocketFrame();
    }
}
