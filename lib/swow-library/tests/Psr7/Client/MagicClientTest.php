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
use Swow\Psr7\Client\ClientRequestException;
use Swow\Psr7\Client\MagicClient;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\Sync\WaitReference;

use function dechex;
use function ord;
use function pack;
use function str_contains;
use function strlen;

/**
 * @internal
 */
#[CoversClass(MagicClient::class)]
final class MagicClientTest extends TestCase
{
    public function testStreamSupportsPostMethod(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();

        $rawRequest = '';
        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, &$rawRequest, $wr): void {
            $connection = $server->accept();
            $rawRequest = $connection->recvString();

            $payload1 = "data: first\n\n";
            $payload2 = "data: second\n\n";
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Content-Type: text/event-stream\r\n" .
                "Transfer-Encoding: chunked\r\n" .
                "Connection: close\r\n" .
                "\r\n" .
                dechex(strlen($payload1)) . "\r\n{$payload1}\r\n" .
                dechex(strlen($payload2)) . "\r\n{$payload2}\r\n" .
                "0\r\n\r\n"
            );
            $connection->close();
        });

        $client = new MagicClient();
        $events = iterator_to_array($client->stream(
            "http://{$server->getSockAddress()}:{$server->getSockPort()}/events",
            [
                'method' => 'POST',
                'json' => ['ping' => 1],
            ]
        ), false);

        $this->assertTrue(str_contains($rawRequest, 'POST /events HTTP/1.1'));
        $this->assertCount(2, $events);
        $this->assertSame('first', $events[0]->data);
        $this->assertSame('second', $events[1]->data);

        $wr::wait($wr);
    }

    public function testRequestSupportsHttpProxyAbsoluteForm(): void
    {
        $proxyServer = new Socket(Socket::TYPE_TCP);
        $proxyServer->bind('127.0.0.1')->listen();

        $rawRequest = '';
        $wr = new WaitReference();
        Coroutine::run(static function () use ($proxyServer, &$rawRequest, $wr): void {
            $connection = $proxyServer->accept();
            $rawRequest = $connection->recvString();
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Content-Length: 2\r\n" .
                "Connection: close\r\n" .
                "\r\n" .
                'OK'
            );
            $connection->close();
        });

        $client = new MagicClient();
        $response = $client->request('GET', 'http://example.com/proxy-check?x=1', [
            'proxy' => [
                'type' => 'http',
                'host' => $proxyServer->getSockAddress(),
                'port' => $proxyServer->getSockPort(),
            ],
        ]);

        $this->assertSame(200, $response->getStatusCode());
        $this->assertSame('OK', (string) $response->getBody());
        $this->assertTrue(str_contains($rawRequest, 'GET http://example.com/proxy-check?x=1 HTTP/1.1'));

        $wr::wait($wr);
    }

    public function testSendRequestUsesConfiguredDefaultProxy(): void
    {
        $proxyServer = new Socket(Socket::TYPE_TCP);
        $proxyServer->bind('127.0.0.1')->listen();

        $rawRequest = '';
        $wr = new WaitReference();
        Coroutine::run(static function () use ($proxyServer, &$rawRequest, $wr): void {
            $connection = $proxyServer->accept();
            $rawRequest = $connection->recvString();
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Content-Length: 2\r\n" .
                "Connection: close\r\n" .
                "\r\n" .
                'OK'
            );
            $connection->close();
        });

        $client = (new MagicClient())->setProxy([
            'type' => 'http',
            'host' => $proxyServer->getSockAddress(),
            'port' => $proxyServer->getSockPort(),
        ]);
        $request = Psr7::createRequest('GET', 'http://example.com/default-proxy');
        $response = $client->sendRequest($request);

        $this->assertSame(200, $response->getStatusCode());
        $this->assertSame('OK', (string) $response->getBody());
        $this->assertTrue(str_contains($rawRequest, 'GET http://example.com/default-proxy HTTP/1.1'));

        $wr::wait($wr);
    }

    public function testRequestSupportsSocks5Proxy(): void
    {
        $proxyServer = new Socket(Socket::TYPE_TCP);
        $proxyServer->bind('127.0.0.1')->listen();

        $rawRequest = '';
        $wr = new WaitReference();
        Coroutine::run(static function () use ($proxyServer, &$rawRequest, $wr): void {
            $connection = $proxyServer->accept();

            $greetingHead = $connection->readString(2);
            $methodCount = ord($greetingHead[1]);
            $connection->readString($methodCount);
            $connection->send("\x05\x00");

            $connectHead = $connection->readString(4);
            $addressType = ord($connectHead[3]);
            if ($addressType === 0x03) {
                $hostLength = ord($connection->readString(1));
                $connection->readString($hostLength);
            } elseif ($addressType === 0x01) {
                $connection->readString(4);
            } elseif ($addressType === 0x04) {
                $connection->readString(16);
            }
            $connection->readString(2);

            $connection->send("\x05\x00\x00\x01\x7f\x00\x00\x01" . pack('n', 0));
            $rawRequest = $connection->recvString();
            $connection->send(
                "HTTP/1.1 200 OK\r\n" .
                "Content-Length: 2\r\n" .
                "Connection: close\r\n" .
                "\r\n" .
                'OK'
            );
            $connection->close();
        });

        $client = new MagicClient();
        $response = $client->request('GET', 'http://example.com/socks-check', [
            'proxy' => [
                'type' => 'socks5',
                'host' => $proxyServer->getSockAddress(),
                'port' => $proxyServer->getSockPort(),
            ],
        ]);

        $this->assertSame(200, $response->getStatusCode());
        $this->assertSame('OK', (string) $response->getBody());
        $this->assertTrue(str_contains($rawRequest, 'GET /socks-check HTTP/1.1'));

        $wr::wait($wr);
    }

    public function testSocks5UnsupportedAuthMethodThrowsClearException(): void
    {
        $proxyServer = new Socket(Socket::TYPE_TCP);
        $proxyServer->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($proxyServer, $wr): void {
            $connection = $proxyServer->accept();
            $greetingHead = $connection->readString(2);
            $methodCount = ord($greetingHead[1]);
            if ($methodCount > 0) {
                $connection->readString($methodCount);
            }
            $connection->send("\x05\x01");
            $connection->close();
        });

        $client = new MagicClient();
        $this->expectException(ClientRequestException::class);
        $this->expectExceptionMessage('unsupported auth method');
        $client->request('GET', 'http://example.com/socks-fail', [
            'proxy' => [
                'type' => 'socks5',
                'host' => $proxyServer->getSockAddress(),
                'port' => $proxyServer->getSockPort(),
            ],
        ]);

        $wr::wait($wr);
    }
}
