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
use Swow\Channel;
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
        foreach ([1, 124, 125, 126, 127, Buffer::COMMON_SIZE - 1, Buffer::COMMON_SIZE, Buffer::COMMON_SIZE + 1, 65535, 65536] as $length) {
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

    #[DataProvider('provideEmptyMaskedFramesRespectAutomaticUnmaskingCases')]
    public function testEmptyMaskedFramesRespectAutomaticUnmasking(int $opcode, bool $autoUnmask): void
    {
        $this->connection->setAutoUnmask($autoUnmask);
        $this->client->sendWebSocketFrame(Psr7::createWebSocketFrame($opcode, '', mask: 'mask'));
        $frame = $this->connection->recvWebSocketFrameEntity();
        $this->assertSame($opcode, $frame->getOpcode());
        $this->assertSame(0, $frame->getPayloadLength());
        $this->assertNull($frame->payloadData);
        $this->assertSame(!$autoUnmask, $frame->getMask());
        $this->assertSame($autoUnmask ? '' : 'mask', $frame->getMaskingKey());
    }

    /** @return iterable<string, array{int, bool}> */
    public static function provideEmptyMaskedFramesRespectAutomaticUnmaskingCases(): iterable
    {
        foreach ([Opcode::TEXT, Opcode::BINARY, Opcode::PING, Opcode::PONG, Opcode::CLOSE] as $opcode) {
            foreach ([true, false] as $autoUnmask) {
                yield "{$opcode} automatic " . (int) $autoUnmask => [$opcode, $autoUnmask];
            }
        }
    }

    #[DataProvider('provideEveryExtendedHeaderSplitWaitsForTheRemainingBytesCases')]
    public function testEveryExtendedHeaderSplitWaitsForTheRemainingBytes(int $length, int $split): void
    {
        $payload = str_repeat('x', $length);
        $wire = Psr7::createWebSocketBinaryFrame($payload, mask: 'mask')->toString();
        $this->client->getSocket()->send(substr($wire, 0, $split));
        $result = new Channel(1);
        $reader = Coroutine::run(function () use ($result): void {
            $result->push($this->connection->recvWebSocketFrame());
        });
        try {
            // The remainder does not exist on the wire until the receiver has suspended.
            $this->assertTrue($reader->isAlive());
            $this->client->getSocket()->send(substr($wire, $split) . WebSocket::PONG_FRAME);
            $frame = $result->pop(1000);
            $this->assertSame($payload, (string) $frame->getPayloadData());
            $this->assertFalse($frame->getMask());
            $this->assertSame(Opcode::PONG, $this->connection->recvWebSocketFrame()->getOpcode());
        } finally {
            if ($reader->isAlive()) {
                $reader->kill();
            }
        }
    }

    /** @return iterable<string, array{int, int}> */
    public static function provideEveryExtendedHeaderSplitWaitsForTheRemainingBytesCases(): iterable
    {
        foreach ([126, 65536] as $length) {
            $headerSize = (new Header(payloadLength: $length, maskingKey: 'mask'))->getHeaderSize();
            for ($split = 1; $split < $headerSize; $split++) {
                yield "{$length} at {$split}" => [$length, $split];
            }
        }
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

    #[DataProvider('provideTruncatedBaseExtendedAndMaskHeadersRaiseASocketExceptionCases')]
    public function testTruncatedBaseExtendedAndMaskHeadersRaiseASocketException(int $split): void
    {
        $header = (new Header(fin: true, payloadLength: 65536, maskingKey: 'mask'))->toString();
        $this->client->getSocket()->send(substr($header, 0, $split));
        $this->client->close();
        $this->expectException(SocketException::class);
        $this->connection->recvWebSocketFrame();
    }

    /** @return iterable<string, array{int}> */
    public static function provideTruncatedBaseExtendedAndMaskHeadersRaiseASocketExceptionCases(): iterable
    {
        for ($split = 1; $split < 14; $split++) {
            yield "header ends at {$split}" => [$split];
        }
    }

    public function testUpgradeRequestAndFirstFrameShareTheReceiveBuffer(): void
    {
        $this->client->getSocket()->send("GET /chat HTTP/1.1\r\nHost: example.test\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n"
            . 'Sec-WebSocket-Key: ' . base64_encode('0123456789abcdef') . "\r\nSec-WebSocket-Version: 13\r\n\r\n"
            . Psr7::createWebSocketTextMaskedFrame('first')->toString());
        $request = $this->connection->recvHttpRequest();
        $this->connection->upgradeToWebSocket($request);
        $this->assertSame(Status::SWITCHING_PROTOCOLS, $this->client->recvResponseEntity()->statusCode);
        $frame = $this->connection->recvWebSocketFrame();
        $this->assertSame(Opcode::TEXT, $frame->getOpcode());
        $this->assertSame('first', (string) $frame->getPayloadData());
        $this->assertFalse($frame->getMask());
    }

    public function testUpgradeResponseAndFirstFrameShareTheReceiveBuffer(): void
    {
        $this->connection->getSocket()->send("HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n\r\n"
            . Psr7::createWebSocketTextFrame('first')->toString());
        $this->assertSame(Status::SWITCHING_PROTOCOLS, $this->client->recvResponseEntity()->statusCode);
        $this->assertSame('first', (string) $this->client->recvWebSocketFrame()->getPayloadData());
    }
}
