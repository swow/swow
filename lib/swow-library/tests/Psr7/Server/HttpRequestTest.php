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
use Swow\Http\Http;
use Swow\Http\Protocol\ProtocolException;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\SocketException;

use function strlen;

/** @internal */
#[CoversClass(Server::class)]
#[CoversClass(ServerConnection::class)]
#[CoversClass(ServerRequest::class)]
#[CoversClass(Psr7::class)]
#[CoversClass(Client::class)]
final class HttpRequestTest extends TestCase
{
    private Server $server;

    private Client $client;

    private ServerConnection $connection;

    protected function setUp(): void
    {
        $this->server = new Server();
        $this->server->setTimeout(1000)->setRecvMessageTimeout(1000)->bind('127.0.0.1')->listen();
        $this->client = new Client();
        $this->client->setTimeout(1000)->connect($this->server->getSockAddress(), $this->server->getSockPort());
        $this->connection = $this->server->acceptConnection(1000);
    }

    protected function tearDown(): void
    {
        $this->connection->close();
        $this->client->close();
        $this->server->close();
    }

    public function testPipelinedRequestsKeepTheirOwnMetadataAndBodies(): void
    {
        $body = 'name=Swow&value=a%2Bb';
        $this->client->getSocket()->send(Http::packRequest('POST', '/first?page=2', [
            'Host' => 'example.test',
            'Cookie' => 'token=a+b; literal=a&b',
            'Content-Type' => 'application/x-www-form-urlencoded',
            'Content-Length' => strlen($body),
        ], $body) . Http::packRequest('GET', '/second', [
            'Host' => 'example.test',
            'Connection' => 'close',
        ]));
        $first = $this->connection->recvHttpRequest();
        $this->assertSame('POST', $first->getMethod());
        $this->assertSame('/first?page=2', $first->getRequestTarget());
        $this->assertSame(['page' => '2'], $first->getQueryParams());
        $this->assertSame(['token' => 'a+b', 'literal' => 'a&b'], $first->getCookieParams());
        $this->assertSame(['name' => 'Swow', 'value' => 'a+b'], $first->getParsedBody());
        $this->assertSame($body, (string) $first->getBody());
        $this->assertSame('127.0.0.1', $first->getServerParams()['remote_addr']);
        $this->assertSame($this->client->getSocket()->getSockPort(), $first->getServerParams()['remote_port']);
        $this->assertTrue($first->shouldKeepAlive());
        $this->connection->respond(Status::CREATED, ['X-Result' => 'first'], 'saved');
        $response = Psr7::createResponseFromEntity($this->client->recvResponseEntity());
        $this->assertSame(Status::CREATED, $response->getStatusCode());
        $this->assertSame('first', $response->getHeaderLine('X-Result'));
        $this->assertSame('saved', (string) $response->getBody());
        $second = $this->connection->recvHttpRequest();
        $this->assertSame('GET', $second->getMethod());
        $this->assertSame('/second', $second->getRequestTarget());
        $this->assertSame([], $second->getCookieParams());
        $this->assertSame('', (string) $second->getBody());
        $this->assertFalse($second->shouldKeepAlive());
        $this->connection->error(Status::NOT_FOUND);
        $response = Psr7::createResponseFromEntity($this->client->recvResponseEntity());
        $this->assertSame(Status::NOT_FOUND, $response->getStatusCode());
        $this->assertSame('close', $response->getHeaderLine('Connection'));
        $this->assertStringContainsString('HTTP 404 Not Found', (string) $response->getBody());
        $this->assertSame($body, (string) $first->getBody());
    }

    public function testContentLengthBodyBeyondTheReceiveBufferDoesNotConsumeTheNextRequest(): void
    {
        $body = str_repeat("a\0b", Buffer::COMMON_SIZE);
        $wire = Http::packRequest('POST', '/binary', [
            'Host' => 'example.test',
            'Content-Type' => 'application/octet-stream',
            'Content-Length' => strlen($body),
        ], $body) . Http::packRequest('GET', '/next', ['Host' => 'example.test']);
        $writer = Coroutine::run(function () use ($wire): void {
            $this->client->getSocket()->send($wire);
        });
        try {
            $request = $this->connection->recvHttpRequest();
            $this->assertSame($body, (string) $request->getBody());
            $this->assertSame((string) strlen($body), $request->getHeaderLine('Content-Length'));
            $this->assertSame('/next', $this->connection->recvHttpRequest()->getRequestTarget());
            $this->assertSame($body, (string) $request->getBody());
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
        }
    }

    public function testChunkedBodyLengthLimitCountsAllChunks(): void
    {
        $this->server->setMaxContentLength(4);
        $this->client->getSocket()->send("POST / HTTP/1.1\r\nHost: example.test\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n2\r\nde\r\n0\r\n\r\n");
        $this->expectException(ProtocolException::class);
        $this->expectExceptionCode(Status::REQUEST_ENTITY_TOO_LARGE);
        $this->connection->recvHttpRequest();
    }

    public function testIncompleteHeadersReportTheirParserContext(): void
    {
        $this->client->getSocket()->send("GET / HTTP/1.1\r\nHost: example.test\r\nX-Incomplete:");
        $this->client->close();
        $this->expectException(SocketException::class);
        $this->expectExceptionMessage('parser finish failed');
        $this->connection->recvHttpRequest();
    }

    #[DataProvider('provideInvalidMessagesAreRejectedCases')]
    public function testInvalidMessagesAreRejected(string $wire, int $maxHeaders, int $maxBody, int $status): void
    {
        $this->server->setMaxHeaderLength($maxHeaders)->setMaxContentLength($maxBody);
        $this->client->getSocket()->send($wire);
        $this->expectException(ProtocolException::class);
        $this->expectExceptionCode($status);
        $this->connection->recvHttpRequest();
    }

    /** @return iterable<string, array{string, int, int, int}> */
    public static function provideInvalidMessagesAreRejectedCases(): iterable
    {
        yield 'oversized body' => ["POST / HTTP/1.1\r\nHost: example.test\r\nContent-Length: 5\r\n\r\n12345", 8192, 4, Status::REQUEST_ENTITY_TOO_LARGE];
        yield 'oversized headers' => ["GET / HTTP/1.1\r\nHost: example.test\r\nX-Long: " . str_repeat('x', 256) . "\r\n\r\n", 64, 8192, Status::REQUEST_HEADER_FIELDS_TOO_LARGE];
        yield 'oversized target' => ['GET /' . str_repeat('x', 256) . " HTTP/1.1\r\nHost: example.test\r\n\r\n", 64, 8192, Status::REQUEST_URI_TOO_LARGE];
        yield 'malformed protocol' => ["INVALID HTTP\r\n\r\n", 8192, 8192, Status::BAD_REQUEST];
    }
}
