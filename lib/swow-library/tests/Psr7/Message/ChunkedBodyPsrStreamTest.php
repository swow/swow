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

namespace Swow\Tests\Psr7\Message;

use InvalidArgumentException;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use RuntimeException;
use Swow\Http\Protocol\ChunkedBodyState;
use Swow\Http\Protocol\ChunkedBodyStream;
use Swow\Http\Protocol\ProtocolException;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ChunkedBodyPsrStream;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Psr7;
use Swow\Socket;

use const SEEK_CUR;
use const SEEK_END;

/** @internal */
#[CoversClass(ChunkedBodyState::class)]
#[CoversClass(ChunkedBodyStream::class)]
#[CoversClass(ChunkedBodyPsrStream::class)]
#[CoversClass(Response::class)]
#[CoversClass(Client::class)]
#[CoversClass(Psr7::class)]
final class ChunkedBodyPsrStreamTest extends TestCase
{
    public function testPartialChunkReturnsBeforeTheRestIsSent(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();
        $client = new Client();
        $client->setTimeout(1000)->setStreamingChunkedResponse(true);
        $peer = null;
        try {
            $client->connect($server->getSockAddress(), $server->getSockPort());
            $peer = $server->accept(1000);
            $peer->send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n6\r\nab");
            $response = Psr7::createResponseFromEntity($client->recvResponseEntity());
            $stream = $response->getBody();
            $this->assertSame('chunked', $response->getHeaderLine('Transfer-Encoding'));
            $this->assertFalse($response->hasHeader('Content-Length'));
            // The producer cannot send the remainder until this read has returned.
            $this->assertSame('ab', $stream->read(6));
            $this->assertFalse($stream->getMetadata('complete'));
            $this->assertSame(2, $stream->getMetadata('buffered_bytes'));
            $peer->send("cdef\r\n0\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n");
            $this->assertSame('cdef', $stream->read(4));
            $this->assertSame('', $stream->getContents());
            $this->assertTrue($stream->eof());
            $this->assertSame('6', $response->getHeaderLine('Content-Length'));
            $this->assertSame('abcdef', (string) $stream);
            $this->assertSame(Status::NO_CONTENT, $client->recvResponseEntity()->statusCode);
        } finally {
            $client->close();
            $peer?->close();
            $server->close();
        }
    }

    #[DataProvider('provideUnfinishedBodyCannotLeaveTheConnectionReusableCases')]
    public function testUnfinishedBodyCannotLeaveTheConnectionReusable(string $action): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();
        $client = new Client();
        $client->setTimeout(1000)->setStreamingChunkedResponse(true);
        $peer = null;
        try {
            $client->connect($server->getSockAddress(), $server->getSockPort());
            $peer = $server->accept(1000);
            $peer->send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
            $stream = Psr7::createResponseFromEntity($client->recvResponseEntity())->getBody();
            if ($action === 'receive') {
                try {
                    $client->recvResponseEntity();
                    $this->fail('An unfinished body must prevent parsing the next response');
                } catch (ProtocolException $exception) {
                    $this->assertSame(Status::BAD_REQUEST, $exception->getCode());
                    $this->assertStringContainsString('Previous streaming chunked body was not fully consumed', $exception->getMessage());
                }
            } elseif ($action === 'detach') {
                $this->assertNull($stream->detach());
            } else {
                $stream->close();
                $stream->close();
            }
            $this->assertFalse($client->getSocket()->isAvailable());
            $this->assertFalse($client->shouldKeepAlive());
        } finally {
            $client->close();
            $peer?->close();
            $server->close();
        }
    }

    /** @return iterable<string, array{string}> */
    public static function provideUnfinishedBodyCannotLeaveTheConnectionReusableCases(): iterable
    {
        yield 'close' => ['close'];
        yield 'detach' => ['detach'];
        yield 'receive next response' => ['receive'];
    }

    public function testStreamingBodyReplayCompletionAndNextResponse(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();
        $client = new Client();
        $client->setTimeout(1000)->setStreamingChunkedResponse(true);
        $peer = null;
        try {
            $client->connect($server->getSockAddress(), $server->getSockPort());
            $peer = $server->accept(1000);
            $peer->send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                . "3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n"
                . "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\ntwo");
            $response = Psr7::createResponseFromEntity($client->recvResponseEntity());
            $stream = $response->getBody();
            $this->assertInstanceOf(ChunkedBodyPsrStream::class, $stream);
            $completed = 0;
            $stream->getChunkedBodyStream()->onCompleted(static function () use (&$completed): void {
                $completed++;
            });
            $this->assertTrue($stream->isReadable());
            $this->assertTrue($stream->isSeekable());
            $this->assertFalse($stream->isWritable());
            $this->assertSame('', $stream->read(0));
            $this->assertSame('ab', $stream->read(2));
            $this->assertSame(2, $stream->tell());
            $stream->rewind();
            $this->assertSame('abc', $stream->read(3));
            $this->assertSame('def', $stream->getContents());
            $this->assertTrue($stream->eof());
            $this->assertSame(6, $stream->getSize());
            $this->assertSame('abcdef', $stream->toString());
            $this->assertSame('abcdef', (string) $stream);
            $stream->seek(-2, SEEK_END);
            $this->assertSame('ef', $stream->read(2));
            $stream->seek(-1, SEEK_CUR);
            $this->assertSame('f', $stream->read(1));
            $this->assertSame(1, $completed);
            $stream->getChunkedBodyStream()->onCompleted(static function () use (&$completed): void {
                $completed++;
            });
            $this->assertSame(2, $completed);
            $this->assertTrue($stream->getMetadata('COMPLETE'));
            $this->assertSame(6, $stream->getMetadata()['buffered_bytes']);
            $this->assertNull($stream->getMetadata(42));
            $this->assertNull($stream->getMetadata('missing'));
            $this->assertSame('6', $response->getHeaderLine('Content-Length'));
            $this->assertFalse($response->hasHeader('Transfer-Encoding'));
            $this->assertSame('two', (string) Psr7::createResponseFromEntity($client->recvResponseEntity())->getBody());
            try {
                $stream->seek(-1);
                $this->fail('Negative offsets must fail');
            } catch (InvalidArgumentException $exception) {
                $this->assertSame('Offset is overflow', $exception->getMessage());
            }
            try {
                $stream->write('data');
                $this->fail('A received body must remain read-only');
            } catch (RuntimeException $exception) {
                $this->assertSame('Cannot write to a read-only stream', $exception->getMessage());
            }
            $this->assertNull($stream->detach());
            $stream->close();
            $this->assertFalse($stream->isReadable());
            $this->assertSame('', (string) $stream);
            $this->assertSame(2, $completed);
            $this->expectException(RuntimeException::class);
            $this->expectExceptionMessage('Stream is closed');
            $stream->read(1);
        } finally {
            $client->close();
            $peer?->close();
            $server->close();
        }
    }
}
