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

use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Client\ClientNetworkException;
use Swow\Psr7\Message\Request as HttpRequest;
use Swow\Psr7\Server\Server;
use Swow\Socket;
use Swow\Sync\WaitReference;

use function usleep;

/**
 * @covers \Swow\Psr7\Client\Client
 * @internal
 */
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
}
