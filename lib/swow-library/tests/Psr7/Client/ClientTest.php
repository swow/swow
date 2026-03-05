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
use Swow\Coroutine;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Client\ClientNetworkException;
use Swow\Psr7\Client\ClientRequestException;
use Swow\Psr7\Message\Request as HttpRequest;
use Swow\Psr7\Server\Server;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

use function str_contains;
use function strlen;
use function usleep;

/**
 * @internal
 */
#[CoversClass(Client::class)]
final class ClientTest extends TestCase
{
    /** This causes HttpParser fall into dead-loop before */
    public function testBadNetwork(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->accept();
            $connection->recvString();
            $connection->send("HTTP/1.1 200 OK\r\nContent-Length: ");
            usleep(10 * 1000);
            $connection->send("0\r\nConnection");
            usleep(10 * 1000);
            $connection->send(": closed\r\n\r\n");
        });

        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $response = $client->sendRequest(new HttpRequest());
        $this->assertSame($response->getStatusCode(), Status::OK);

        $wr::wait($wr);
    }

    public function testConnectionClosedByPeer(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->accept();
            $connection->close();
        });

        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        try {
            $client->sendRequest(new HttpRequest());
            $this->fail('Never here');
        } catch (ClientNetworkException $exception) {
            /* ExceptionFaker works */
            $this->assertNull($exception->getPrevious());
        }

        $wr::wait($wr);
    }

    public function testFinishWithNoContentLength(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->acceptConnection();
            $connection->recvHttpRequest();
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Host: {$server->getSockAddress()}:{$server->getSockPort()}\r\n" .
                "Connection: close\r\n" .
                "Content-type: text/html; charset=UTF-8\r\n" .
                "\r\n"
            );
            $connection->close();
        });

        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $response = $client->sendRequest(new HttpRequest());
        $this->assertSame(Status::OK, $response->getStatusCode());

        $wr::wait($wr);
    }

    public function testStreamingChunkedBodyWillNormalizeHeadersAfterCompletion(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->accept();
            $connection->recvString();
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Transfer-Encoding: chunked\r\n" .
                "Connection: keep-alive\r\n" .
                "\r\n" .
                "5\r\nhello\r\n" .
                "6\r\n world\r\n" .
                "0\r\n\r\n"
            );
        });

        $client = (new Client())
            ->setStreamingChunkedResponse(true)
            ->connect($server->getSockAddress(), $server->getSockPort());
        $response = $client->sendRequest(new HttpRequest());
        $this->assertSame('chunked', strtolower($response->getHeaderLine('transfer-encoding')));

        $body = (string) $response->getBody();
        $this->assertSame('hello world', $body);
        $this->assertSame((string) strlen($body), $response->getHeaderLine('content-length'));
        $this->assertSame('', $response->getHeaderLine('transfer-encoding'));

        $wr::wait($wr);
    }

    public function testStreamingChunkedBodyMustBeConsumedBeforeNextRequest(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->accept();
            $connection->recvString();
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Transfer-Encoding: chunked\r\n" .
                "Connection: keep-alive\r\n" .
                "\r\n" .
                "5\r\nhello\r\n"
            );
            usleep(200 * 1000);
            $connection->close();
        });

        $client = (new Client())
            ->setStreamingChunkedResponse(true)
            ->connect($server->getSockAddress(), $server->getSockPort());
        $response = $client->sendRequest(new HttpRequest());
        $this->assertSame('hello', $response->getBody()->read(5));

        try {
            $client->sendRequest(new HttpRequest());
            $this->fail('Never here');
        } catch (ClientRequestException $exception) {
            $this->assertStringContainsString('Previous streaming chunked body was not fully consumed', $exception->getMessage());
            $this->assertStringContainsString('buffered_bytes=', $exception->getMessage());
        }

        $wr::wait($wr);
    }

    public function testStreamingChunkedSocketExceptionContainsContext(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->accept();
            $connection->recvString();
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Transfer-Encoding: chunked\r\n" .
                "Connection: keep-alive\r\n" .
                "\r\n"
            );
            usleep(300 * 1000);
            $connection->close();
        });

        $client = (new Client())
            ->setStreamingChunkedResponse(true)
            ->setRecvMessageTimeout(50)
            ->connect($server->getSockAddress(), $server->getSockPort());
        $response = $client->sendRequest(new HttpRequest(), 50);
        try {
            $response->getBody()->read(1);
            $this->fail('Never here');
        } catch (SocketException $exception) {
            $message = $exception->getMessage();
            $this->assertTrue(str_contains($message, 'streaming-chunked-context:'));
            $this->assertTrue(str_contains($message, 'parsed_offset='));
            $this->assertTrue(str_contains($message, 'buffer_length='));
            $this->assertTrue(str_contains($message, 'body_buffered='));
            $this->assertTrue(str_contains($message, 'current_chunk_length='));
        }

        $wr::wait($wr);
    }
}
