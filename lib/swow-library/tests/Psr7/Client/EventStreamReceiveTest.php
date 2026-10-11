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
use Swow\Http\Protocol\ChunkedBodyState;
use Swow\Http\Protocol\ChunkedBodyStream;
use Swow\Http\Protocol\ProtocolException;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ChunkedBodyPsrStream;
use Swow\Psr7\Message\EventStreamDecoder;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\SocketException;

use function strlen;

use const JSON_THROW_ON_ERROR;

/** @internal */
#[CoversClass(Client::class)]
#[CoversClass(Response::class)]
#[CoversClass(Psr7::class)]
#[CoversClass(EventStreamDecoder::class)]
#[CoversClass(ChunkedBodyState::class)]
#[CoversClass(ChunkedBodyStream::class)]
#[CoversClass(ChunkedBodyPsrStream::class)]
final class EventStreamReceiveTest extends TestCase
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
        $this->peer->send("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nTransfer-Encoding: chunked\r\n\r\n");
    }

    protected function tearDown(): void
    {
        $this->client->close();
        $this->peer->close();
        $this->server->close();
    }

    private static function chunk(string $data): string
    {
        return dechex(strlen($data)) . "\r\n{$data}\r\n";
    }

    #[DataProvider('provideModelDeltasHeartbeatsAndEndMarkersCrossChunkAndUtf8BoundariesCases')]
    public function testModelDeltasHeartbeatsAndEndMarkersCrossChunkAndUtf8Boundaries(int $readSize): void
    {
        $json = '{"delta":{"content":"你🙂","reasoning":"thinking"}}';
        $sse = ": heartbeat\r\n\r\nevent: token\r\ndata: {$json}\r\n\r\ndata: [DONE]\r\n\r\n";
        $wire = '';
        foreach (str_split($sse, 3) as $part) {
            $wire .= self::chunk($part);
        }
        $this->peer->send($wire . "0\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n");
        $response = Psr7::createResponseFromEntity($this->client->recvResponseEntity());
        $events = iterator_to_array(Psr7::readEventStream($response->getBody(), $readSize), false);
        $this->assertCount(2, $events);
        $this->assertSame('token', $events[0]->event);
        $this->assertSame($json, $events[0]->data);
        $this->assertSame('你🙂', json_decode($events[0]->data, true, flags: JSON_THROW_ON_ERROR)['delta']['content']);
        $this->assertSame('[DONE]', $events[1]->data);
        $this->assertSame((string) strlen($sse), $response->getHeaderLine('Content-Length'));
        $this->assertSame(Status::NO_CONTENT, $this->client->recvResponseEntity()->statusCode);
    }

    /** @return iterable<string, array{int}> */
    public static function provideModelDeltasHeartbeatsAndEndMarkersCrossChunkAndUtf8BoundariesCases(): iterable
    {
        foreach ([1, 2, 7, Buffer::COMMON_SIZE] as $size) {
            yield (string) $size => [$size];
        }
    }

    public function testFirstEventIsAvailableBeforeTheProducerCanSendTheNextEvent(): void
    {
        $this->peer->send(self::chunk("data: first\n\n"));
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $events = Psr7::readEventStream($body);
        // The producer runs in this coroutine: waiting for later bytes would time out.
        $events->rewind();
        $this->assertSame('first', $events->current()->data);
        $this->assertFalse($body->getMetadata('complete'));
        $this->peer->send(self::chunk("event: error\ndata: {\"message\":\"provider stopped\"}\n\n") . "0\r\n\r\n");
        $events->next();
        $this->assertSame('error', $events->current()->event);
        $this->assertSame('{"message":"provider stopped"}', $events->current()->data);
        $events->next();
        $this->assertFalse($events->valid());
        $this->assertTrue($body->getMetadata('complete'));
    }

    public function testLongModelOutputUsesTheConfiguredLimitAndPreservesTheNextResponse(): void
    {
        $record = 'data: {"content":"' . str_repeat('x', Buffer::COMMON_SIZE) . "\"}\n\n";
        $count = 1024;
        $this->client->setMaxContentLength(strlen($record) * $count + strlen("data: [DONE]\n\n"));
        $writer = Coroutine::run(function () use ($record, $count): void {
            for ($i = 0; $i < $count; $i++) {
                $this->peer->send(self::chunk($record));
            }
            $this->peer->send(self::chunk("data: [DONE]\n\n") . "0\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n");
        });
        try {
            $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
            $received = 0;
            foreach (Psr7::readEventStream($body) as $event) {
                if ($event->data === '[DONE]') {
                    $this->assertSame($count, $received);
                } else {
                    $this->assertSame(Buffer::COMMON_SIZE, strlen(json_decode($event->data, true, flags: JSON_THROW_ON_ERROR)['content']));
                    $received++;
                }
            }
            $this->assertSame($count, $received);
            $this->assertTrue($body->getMetadata('complete'));
            $this->assertSame(Status::NO_CONTENT, $this->client->recvResponseEntity()->statusCode);
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
        }
    }

    public function testCumulativeStreamLimitStillRejectsTooMuchOutput(): void
    {
        $record = "data: token\n\n";
        $this->client->setMaxContentLength(strlen($record) * 2);
        $this->peer->send(str_repeat(self::chunk($record), 3) . "0\r\n\r\n");
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        try {
            $this->expectException(ProtocolException::class);
            $this->expectExceptionCode(Status::REQUEST_ENTITY_TOO_LARGE);
            iterator_to_array(Psr7::readEventStream($body, strlen($record)), false);
        } finally {
            $body->close();
            $this->assertFalse($this->client->getSocket()->isAvailable());
        }
    }

    public function testInterruptedJsonCannotBecomeACompletedEvent(): void
    {
        $this->peer->send(self::chunk("data: first\n\n") . "20\r\ndata: {\"content\":\"");
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        $events = Psr7::readEventStream($body, 4);
        $events->rewind();
        $this->assertSame('first', $events->current()->data);
        $this->peer->close();
        try {
            $this->expectException(SocketException::class);
            $events->next();
        } finally {
            $this->assertFalse($body->getMetadata('complete'));
            $this->assertFalse($this->client->shouldKeepAlive());
            $body->close();
        }
    }

    public function testStoppingConsumptionClosesTheUnfinishedConnection(): void
    {
        $this->peer->send(self::chunk("data: first\n\n"));
        $body = Psr7::createResponseFromEntity($this->client->recvResponseEntity())->getBody();
        foreach (Psr7::readEventStream($body) as $event) {
            $this->assertSame('first', $event->data);
            break;
        }
        $body->close();
        $body->close();
        $this->assertFalse($body->getMetadata('complete'));
        $this->assertFalse($this->client->shouldKeepAlive());
        $this->assertFalse($this->client->getSocket()->isAvailable());
    }
}
