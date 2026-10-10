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

namespace Swow\Tests\Psr7\Client;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use RuntimeException;
use Swow\Http\Protocol\ChunkedBodyState;
use Swow\Http\Protocol\ChunkedBodyStream;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ChunkedBodyPsrStream;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Psr7;
use Swow\Socket;

/** @internal */
#[CoversClass(Client::class)]
#[CoversClass(Response::class)]
#[CoversClass(Psr7::class)]
#[CoversClass(ChunkedBodyState::class)]
#[CoversClass(ChunkedBodyStream::class)]
#[CoversClass(ChunkedBodyPsrStream::class)]
final class ClientRecreateTest extends TestCase
{
    private Socket $server;

    private Socket $peer;

    private ?Socket $nextPeer = null;

    private Client $client;

    protected function setUp(): void
    {
        $this->server = (new Socket(Socket::TYPE_TCP))->setTimeout(1000)->bind('127.0.0.1')->listen();
        $this->client = (new Client())->setTimeout(1000)->connect('127.0.0.1', $this->server->getSockPort());
        $this->peer = $this->server->accept();
    }

    protected function tearDown(): void
    {
        $this->client->close();
        $this->peer->close();
        $this->nextPeer?->close();
        $this->server->close();
    }

    private function reconnect(): void
    {
        $this->client->setTimeout(1000)->connect('127.0.0.1', $this->server->getSockPort());
        $this->nextPeer = $this->server->accept();
        $this->nextPeer->send("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nnew");
    }

    public function testRecreateDiscardsThePreviousReceiveOffsetAndKeepAliveState(): void
    {
        $this->peer->send("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nold"
            . "HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\nstale!");
        $this->assertSame('old', (string) $this->client->recvResponseEntity()->body);
        $this->assertTrue($this->client->shouldKeepAlive());
        $this->assertSame($this->client, $this->client->recreate(Client::TYPE_TCP4));
        $this->assertSame(Client::TYPE_TCP4, $this->client->getSocketType());
        $this->assertFalse($this->client->isEstablished());
        $this->assertFalse($this->client->shouldKeepAlive());
        $this->reconnect();
        $this->assertSame('new', (string) $this->client->recvResponseEntity()->body);
    }

    public function testAnAbandonedStreamCannotReadOrCloseTheRecreatedConnection(): void
    {
        $this->client->setStreamingChunkedResponse(true);
        $this->peer->send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n6\r\nab");
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $this->assertSame('ab', $body->read(6));
        $this->assertFalse($body->getMetadata('complete'));
        $this->assertSame($this->client, $this->client->recreate());
        $this->reconnect();
        try {
            $body->getContents();
            $this->fail('An abandoned body must not consume the new connection');
        } catch (RuntimeException $exception) {
            $this->assertSame('Stream is closed', $exception->getMessage());
        }
        $body->close();
        $this->assertTrue($this->client->isEstablished());
        $this->assertSame('new', (string) $this->client->recvResponseEntity()->body);
    }

    public function testACompletedStreamRemainsReplayableAfterRecreatingTheClient(): void
    {
        $this->client->setStreamingChunkedResponse(true);
        $this->peer->send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nold\r\n0\r\n\r\n");
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $this->assertSame('old', $body->getContents());
        $this->assertTrue($body->getMetadata('complete'));
        $this->client->recreate();
        $this->reconnect();
        $this->assertSame('old', $body->toString());
        $body->close();
        $this->assertTrue($this->client->isEstablished());
        $this->assertSame('new', (string) $this->client->recvResponseEntity()->body);
    }
}
