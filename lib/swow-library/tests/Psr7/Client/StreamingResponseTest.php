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
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Buffer;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Http\ParserException;
use Swow\Http\Protocol\ChunkedBodyState;
use Swow\Http\Protocol\ChunkedBodyStream;
use Swow\Http\Protocol\ProtocolException;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ChunkedBodyPsrStream;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\SocketException;

use function strlen;

/** @internal */
#[CoversClass(Client::class)]
#[CoversClass(Response::class)]
#[CoversClass(Psr7::class)]
#[CoversClass(ChunkedBodyState::class)]
#[CoversClass(ChunkedBodyStream::class)]
#[CoversClass(ChunkedBodyPsrStream::class)]
final class StreamingResponseTest extends TestCase
{
    private Socket $server;

    private Socket $peer;

    private Client $client;

    protected function setUp(): void
    {
        $this->server = (new Socket(Socket::TYPE_TCP))->setTimeout(1000)->bind('127.0.0.1')->listen();
        $this->client = (new Client())->setTimeout(1000)->setStreamingChunkedResponse(true);
        $this->client->connect('127.0.0.1', $this->server->getSockPort());
        $this->peer = $this->server->accept();
        $this->peer->send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n");
    }

    protected function tearDown(): void
    {
        $this->client->close();
        $this->peer->close();
        $this->server->close();
    }

    public function testLargePartialChunkCanBeCompletedAndReplayedBeforeTheNextResponse(): void
    {
        $response = Psr7::createResponseFromEntity($this->client->recvResponseEntity());
        $body = $response->getBody();
        $payload = str_repeat("a\0\xffb", Buffer::COMMON_SIZE);
        $this->peer->send(dechex(strlen($payload)) . "\r\n" . substr($payload, 0, 2));
        $this->assertSame(substr($payload, 0, 2), $body->read(strlen($payload)));
        $this->assertSame(2, $body->getMetadata('buffered_bytes'));
        $this->assertFalse($body->getMetadata('complete'));
        $writer = Coroutine::run(function () use ($payload): void {
            $this->peer->send(substr($payload, 2) . "\r\n1\r\nz\r\n0\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n");
        });
        try {
            $this->assertSame(substr($payload, 2) . 'z', $body->getContents());
            $this->assertTrue($body->eof());
            $this->assertSame($payload . 'z', $body->toString());
            $this->assertSame((string) (strlen($payload) + 1), $response->getHeaderLine('Content-Length'));
            $this->assertFalse($response->hasHeader('Transfer-Encoding'));
            $this->assertSame(Status::NO_CONTENT, $this->client->recvResponseEntity()->statusCode);
            $body->close();
            $body->close();
            $this->assertTrue($this->client->getSocket()->isAvailable());
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
        }
    }

    #[DataProvider('provideMalformedChunkFramingClosesTheConnectionCases')]
    public function testMalformedChunkFramingClosesTheConnection(string $wire): void
    {
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $this->peer->send($wire);
        try {
            $body->getContents();
            $this->fail('Malformed chunk framing must fail');
        } catch (ProtocolException $exception) {
            $this->assertSame(Status::BAD_REQUEST, $exception->getCode());
            $this->assertInstanceOf(ParserException::class, $exception->getPrevious());
            $this->assertFalse($this->client->shouldKeepAlive());
            $this->assertFalse($this->client->getSocket()->isAvailable());
            $this->assertFalse($body->getMetadata('complete'));
        }
    }

    /** @return iterable<string, array{string}> */
    public static function provideMalformedChunkFramingClosesTheConnectionCases(): iterable
    {
        yield 'invalid length' => ["invalid\r\n"];
        yield 'invalid delimiter' => ["3\r\nabc!\r\n0\r\n\r\n"];
    }

    public function testContentLimitCountsSuccessiveStreamedChunks(): void
    {
        $this->client->setMaxContentLength(4);
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $this->peer->send("3\r\nabc\r\n2\r\nde\r\n0\r\n\r\n");
        $this->assertSame('abc', $body->read(3));
        try {
            $body->getContents();
            $this->fail('The total body size must enforce the configured limit');
        } catch (ProtocolException $exception) {
            $this->assertSame(Status::REQUEST_ENTITY_TOO_LARGE, $exception->getCode());
            $this->assertSame(3, $body->getMetadata('buffered_bytes'));
            $this->assertFalse($body->getMetadata('complete'));
            $this->assertFalse($this->client->shouldKeepAlive());
            $this->assertFalse($this->client->getSocket()->isAvailable());
        }
        $body->close();
        $this->assertFalse($this->client->getSocket()->isAvailable());
        $this->assertFalse($this->client->shouldKeepAlive());
    }

    public function testInterruptedChunkIncludesBufferedBodyAndParserContext(): void
    {
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $this->peer->send("6\r\nab");
        $this->assertSame('ab', $body->read(6));
        $this->peer->close();
        try {
            $body->getContents();
            $this->fail('An incomplete chunk must not be reported as a complete body');
        } catch (SocketException $exception) {
            $this->assertStringContainsString('streaming-chunked-context:', $exception->getMessage());
            $this->assertStringContainsString('body_buffered=2', $exception->getMessage());
            $this->assertStringContainsString('current_chunk_length=6', $exception->getMessage());
            $this->assertFalse($this->client->shouldKeepAlive());
            $this->assertFalse($body->getMetadata('complete'));
        }
        $body->close();
    }

    public function testStreamReadTimeoutRetainsItsErrorCodeAndContext(): void
    {
        $this->client->setRecvMessageTimeout(10);
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        try {
            $body->read(1);
            $this->fail('The body read must honor the configured message timeout');
        } catch (SocketException $exception) {
            $this->assertSame(Errno::ETIMEDOUT, $exception->getCode());
            $this->assertStringContainsString('streaming-chunked-context:', $exception->getMessage());
            $this->assertSame(0, $body->getMetadata('buffered_bytes'));
            $this->assertFalse($body->getMetadata('complete'));
        }
        $body->close();
        $this->assertFalse($this->client->getSocket()->isAvailable());
    }

    public function testReadingAChunkRemainderHonorsTheTotalOperationTimeout(): void
    {
        $this->client->setRecvMessageTimeout(50);
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $this->peer->send("a\r\nx");
        $this->assertSame('x', $body->read(1));
        $writer = Coroutine::run(function (): void {
            for ($i = 0; $i < 9; $i++) {
                usleep(10000);
                $this->peer->send('x');
            }
            $this->peer->send("\r\n0\r\n\r\n");
        });
        try {
            $this->expectException(SocketException::class);
            $this->expectExceptionCode(Errno::ETIMEDOUT);
            $body->getContents();
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
            $body->close();
        }
    }

    public function testLaterChunkReadsRetainTheShorterSocketTimeout(): void
    {
        $this->client->setRecvMessageTimeout(100);
        $this->client->getSocket()->setReadTimeout(10);
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $writer = Coroutine::run(function (): void {
            usleep(1000);
            $this->peer->send("1\r\n");
            usleep(30000);
            $this->peer->send("x\r\n0\r\n\r\n");
        });
        try {
            $this->expectException(SocketException::class);
            $this->expectExceptionCode(Errno::ETIMEDOUT);
            $body->getContents();
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
            $body->close();
        }
    }
}
