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
use Swow\Coroutine;
use Swow\Psr7\Client\ClientRequestException;
use Swow\Psr7\Client\MagicClient;
use Swow\Psr7\Psr7;
use Swow\Socket;

use function chr;
use function strlen;

/** @internal */
#[CoversClass(MagicClient::class)]
final class MagicClientSocks5Test extends TestCase
{
    /** @param list<array{string, string}> $exchanges */
    #[DataProvider('provideNegotiationCases')]
    public function testNegotiation(array $exchanges, bool $credentials, ?string $error, string $targetHost = 'example.test', string $proxyType = 'socks5h'): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->setTimeout(1000)->bind('127.0.0.1')->listen();
        $received = [];
        $worker = Coroutine::run(static function () use ($server, $exchanges, $error, &$received): void {
            $connection = $server->accept(1000);
            try {
                foreach ($exchanges as [$expected, $reply]) {
                    $received[] = $connection->readString(strlen($expected), 1000);
                    $connection->send($reply);
                }
                if ($error === null) {
                    $request = '';
                    while (!str_contains($request, "\r\n\r\n")) {
                        $data = $connection->recvString(1024, 1000);
                        if ($data === '') {
                            break;
                        }
                        $request .= $data;
                    }
                    if ($request !== '') {
                        $connection->send("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                    }
                }
            } finally {
                $connection->close();
            }
        });
        $proxy = ['type' => $proxyType, 'host' => $server->getSockAddress(), 'port' => $server->getSockPort(), 'remote_dns' => false];
        if ($credentials) {
            $proxy['username'] = 'user';
            $proxy['password'] = 'secret';
        }
        $client = (new MagicClient())->setConnectTimeout(1000)->setRecvMessageTimeout(1000)->setProxy($proxy);
        $request = Psr7::createRequest('GET', "http://{$targetHost}:8080/target");
        try {
            try {
                $response = $client->sendRequest($request);
                $this->assertNull($error, 'Invalid SOCKS negotiation must not reach HTTP dispatch');
                $this->assertSame('ok', (string) $response->getBody());
            } catch (ClientRequestException $exception) {
                $this->assertNotNull($error);
                $this->assertStringContainsString($error, $exception->getMessage());
                $this->assertSame($request, $exception->getRequest());
            }
            $this->assertSame(array_column($exchanges, 0), $received);
        } finally {
            $client->getClient()->close();
            $server->close();
            if ($worker->isAlive()) {
                $worker->kill();
            }
        }
    }

    /** @return iterable<string, array{list<array{string, string}>, bool, ?string, 3?: string, 4?: string}> */
    public static function provideNegotiationCases(): iterable
    {
        $greeting = "\x05\x02\x00\x02";
        $authentication = "\x01\x04user\x06secret";
        $host = 'example.test';
        $connect = "\x05\x01\x00\x03" . chr(strlen($host)) . $host . pack('n', 8080);
        $prefix = [[$greeting, "\x05\x02"], [$authentication, "\x01\x00"]];
        foreach ([
            'IPv4 reply' => "\x01" . inet_pton('127.0.0.1'),
            'domain reply' => "\x03\x0arelay.test",
            'IPv6 reply' => "\x04" . inet_pton('::1'),
        ] as $name => $address) {
            yield $name => [[...$prefix, [$connect, "\x05\x00\x00" . $address . pack('n', 8081)]], true, null];
        }
        foreach (['127.0.0.1' => "\x01" . inet_pton('127.0.0.1'), '[::1]' => "\x04" . inet_pton('::1')] as $target => $address) {
            $ipConnect = "\x05\x01\x00" . $address . pack('n', 8080);
            yield 'IP target ' . $target => [[...$prefix, [$ipConnect, "\x05\x00\x00\x01" . inet_pton('127.0.0.1') . pack('n', 8081)]], true, null, $target, 'socks5'];
        }
        yield 'wrong method version' => [[[$greeting, "\x04\x00"]], true, 'invalid version in method reply'];
        yield 'no acceptable method' => [[[$greeting, "\x05\xff"]], true, 'no acceptable auth method'];
        yield 'unsupported method' => [[[$greeting, "\x05\x03"]], true, 'unsupported auth method'];
        yield 'unadvertised password authentication' => [[["\x05\x01\x00", "\x05\x02"]], false, 'unadvertised auth method'];
        yield 'wrong authentication version' => [[[$greeting, "\x05\x02"], [$authentication, "\x02\x00"]], true, 'invalid version in auth reply'];
        yield 'rejected credentials' => [[[$greeting, "\x05\x02"], [$authentication, "\x01\x01"]], true, 'username/password rejected'];
        yield 'wrong connect version' => [[...$prefix, [$connect, "\x04\x00\x00\x01"]], true, 'invalid version in response'];
        yield 'connection refused' => [[...$prefix, [$connect, "\x05\x05\x00\x01"]], true, 'reply_code=5'];
        yield 'wrong address type' => [[...$prefix, [$connect, "\x05\x00\x00\x05"]], true, 'invalid address type=5'];
    }
}
