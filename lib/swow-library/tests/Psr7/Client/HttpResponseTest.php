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
use Swow\Channel;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Http\Protocol\ProtocolException;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Client\ClientNetworkException;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\SocketException;

use function strlen;

/** @internal */
#[CoversClass(Client::class)]
#[CoversClass(Response::class)]
#[CoversClass(Psr7::class)]
final class HttpResponseTest extends TestCase
{
    private Socket $server;
    private Socket $peer;
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
        $this->server->close();
    }

    #[DataProvider('provideBodylessStatusesPreserveTheNextResponseCases')]
    public function testBodylessStatusesPreserveTheNextResponse(int $status): void
    {
        $this->peer->send("HTTP/1.1 {$status} Result\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nnext");
        $first = $this->client->recvResponseEntity();
        $this->assertSame($status, $first->statusCode);
        $this->assertNull($first->body);
        $this->assertSame('next', (string) $this->client->recvResponseEntity()->body);
    }

    /** @return iterable<string, array{int}> */
    public static function provideBodylessStatusesPreserveTheNextResponseCases(): iterable
    {
        foreach ([100, 103, 204, 304] as $status) {
            yield (string) $status => [$status];
        }
    }

    public function testCloseDelimitedBodyAccumulatesUntilEof(): void
    {
        $body = str_repeat("a\0b", Buffer::COMMON_SIZE);
        $writer = Coroutine::run(function () use ($body): void {
            $this->peer->send("HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n" . $body);
            $this->peer->close();
        });
        try {
            $response = $this->client->recvResponseEntity();
            $this->assertSame($body, (string) $response->body);
            $this->assertSame(strlen($body), $response->contentLength);
            $this->assertFalse($response->shouldKeepAlive);
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
        }
    }

    public function testCloseDelimitedBodyDoesNotTreatTimeoutAsCompletion(): void
    {
        $this->client->setRecvMessageTimeout(10);
        $this->peer->send("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\npartial");
        $this->expectException(SocketException::class);
        $this->expectExceptionCode(Errno::ETIMEDOUT);
        $this->client->recvResponseEntity();
    }

    public function testCloseDelimitedBodyStillEnforcesTheConfiguredContentLimit(): void
    {
        $this->client->setMaxContentLength(3);
        $this->peer->send("HTTP/1.0 200 OK\r\n\r\ntoolong");
        $this->peer->close();
        $this->expectException(ProtocolException::class);
        $this->expectExceptionCode(Status::REQUEST_ENTITY_TOO_LARGE);
        $this->client->recvResponseEntity();
    }

    public function testTruncatedContentLengthBodyCannotBecomeASuccessfulResponse(): void
    {
        $this->peer->send("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort");
        $this->peer->close();
        $this->expectException(SocketException::class);
        $this->client->recvResponseEntity();
    }

    public function testBufferedLargeChunkWaitsForItsRemainderAndPreservesTheNextResponse(): void
    {
        $payload = str_repeat('x', Buffer::COMMON_SIZE * 2);
        $this->peer->send("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" . dechex(strlen($payload)) . "\r\nx");
        $result = new Channel(1);
        $reader = Coroutine::run(function () use ($result): void {
            $result->push($this->client->recvResponseEntity());
        });
        try {
            $this->assertTrue($reader->isAlive());
            $this->peer->send(substr($payload, 1) . "\r\n0\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n");
            $this->assertSame($payload, (string) $result->pop(1000)->body);
            $this->assertSame(Status::NO_CONTENT, $this->client->recvResponseEntity()->statusCode);
        } finally {
            if ($reader->isAlive()) {
                $reader->kill();
            }
        }
    }

    public function testHeadResponseDoesNotReadTheAdvertisedRepresentationBody(): void
    {
        $this->peer->send("HTTP/1.1 200 OK\r\nContent-Length: 12345\r\n\r\n"
            . "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nnext");
        $response = $this->client->sendRequest(Psr7::createRequest('HEAD', '/metadata'));
        $this->assertSame('12345', $response->getHeaderLine('Content-Length'));
        $this->assertSame('', (string) $response->getBody());
        $this->assertSame('next', (string) $this->client->recvResponseEntity()->body);
    }

    public function testSendRequestReturnsTheFinalResponseAfterInformationalResponses(): void
    {
        $this->peer->send("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 103 Early Hints\r\nLink: </style.css>\r\n\r\n"
            . "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
        $response = $this->client->sendRequest(Psr7::createRequest('POST', '/complete', body: '{}'));
        $this->assertSame(Status::OK, $response->getStatusCode());
        $this->assertSame('ok', (string) $response->getBody());
        $this->assertFalse($response->hasHeader('Link'));
    }

    public function testInformationalResponsesDoNotRestartTheResponseTimeout(): void
    {
        $writer = Coroutine::run(function (): void {
            for ($i = 0; $i < 10; $i++) {
                $this->peer->send("HTTP/1.1 103 Early Hints\r\n\r\n");
                usleep(10000);
            }
            $this->peer->send("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
        });
        try {
            $this->expectException(ClientNetworkException::class);
            $this->expectExceptionCode(Errno::ETIMEDOUT);
            $this->client->sendRequest(Psr7::createRequest('GET', '/hints'), 50);
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
        }
    }
}
