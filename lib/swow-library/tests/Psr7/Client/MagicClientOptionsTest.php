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

use InvalidArgumentException;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Psr7\Client\ClientRequestException;
use Swow\Psr7\Client\MagicClient;
use Swow\Psr7\Server\Server;

/** @internal */
#[CoversClass(MagicClient::class)]
final class MagicClientOptionsTest extends TestCase
{
    public function testHostHeaderCasingCannotOverrideTheTargetUri(): void
    {
        $server = new Server();
        $server->setTimeout(1000)->bind('127.0.0.1')->listen();
        $received = [];
        $worker = Coroutine::run(static function () use ($server, &$received): void {
            $connection = $server->acceptConnection(1000);
            try {
                for ($index = 0; $index < 2; $index++) {
                    $received[] = $connection->recvHttpRequest();
                    $connection->respond(body: 'ok');
                }
            } finally {
                $connection->close();
            }
        });
        $client = (new MagicClient())->setConnectTimeout(1000)->setRecvMessageTimeout(1000);
        $endpoint = $server->getSockAddress() . ':' . $server->getSockPort();
        try {
            $response = $client->get("http://{$endpoint}/first?base=1", [
                'headers' => ['hOsT' => 'untrusted.test', 'X-Value' => 'first'],
                'query' => ['value' => 'a+b'],
                'tls' => null,
            ]);
            $this->assertSame('ok', (string) $response->getBody());
            $connection = $client->getClient();
            $response = $client->post("http://{$endpoint}/second", ['json' => '{"value":0}', 'timeout' => 1000]);
            $this->assertSame('ok', (string) $response->getBody());
            $this->assertSame($connection, $client->getClient());
            $this->assertSame($endpoint, $received[0]->getHeaderLine('Host'));
            $this->assertSame('/first?base=1&value=a%2Bb', $received[0]->getRequestTarget());
            $this->assertSame('first', $received[0]->getHeaderLine('X-Value'));
            $this->assertSame('POST', $received[1]->getMethod());
            $this->assertSame('application/json', $received[1]->getHeaderLine('Content-Type'));
            $this->assertSame('{"value":0}', (string) $received[1]->getBody());
        } finally {
            $client->getClient()->close();
            $server->close();
            if ($worker->isAlive()) {
                $worker->kill();
            }
        }
    }

    /** @param array<string, mixed> $options */
    #[DataProvider('provideInvalidOptionsCases')]
    public function testInvalidOptions(array $options, bool $stream, string $message): void
    {
        $client = new MagicClient();
        try {
            $this->expectException(InvalidArgumentException::class);
            $this->expectExceptionMessage($message);
            if ($stream) {
                $client->stream('http://example.test/', $options);
            } else {
                $client->get('http://example.test/', $options);
            }
        } finally {
            $client->getClient()->close();
        }
    }

    /** @return iterable<string, array{array<string, mixed>, bool, string}> */
    public static function provideInvalidOptionsCases(): iterable
    {
        yield 'proxy type' => [['proxy' => 'http://proxy'], false, 'must be null or an array'];
        yield 'proxy protocol' => [['proxy' => ['type' => 'unknown']], false, 'proxy.type'];
        yield 'proxy host' => [['proxy' => ['type' => 'http']], false, 'proxy.host'];
        $proxy = ['type' => 'socks5', 'host' => 'proxy.test', 'port' => 1080];
        foreach ([0, 65536, '1080'] as $port) {
            yield 'proxy port ' . $port => [['proxy' => ['port' => $port] + $proxy], false, 'proxy.port'];
        }
        yield 'missing password' => [['proxy' => $proxy + ['username' => 'user']], false, 'must be set together'];
        yield 'username type' => [['proxy' => $proxy + ['username' => 1, 'password' => 'secret']], false, 'proxy.username'];
        yield 'password type' => [['proxy' => $proxy + ['username' => 'user', 'password' => 1]], false, 'proxy.password'];
        yield 'credentials length' => [['proxy' => $proxy + ['username' => str_repeat('u', 256), 'password' => 'secret']], false, 'length must be'];
        yield 'tls type' => [['tls' => 'invalid'], false, 'tls'];
        yield 'header type' => [['headers' => 'invalid'], false, 'headers'];
        yield 'body conflict' => [['body' => 'value', 'json' => ['value']], false, 'mutually exclusive'];
        yield 'timeout range' => [['timeout' => -1], false, 'timeout'];
        yield 'timeout type' => [['timeout' => '1000'], false, 'timeout'];
        yield 'read size range' => [['read_size' => 0], true, 'read_size'];
        yield 'read size type' => [['read_size' => '8192'], true, 'read_size'];
    }

    #[DataProvider('provideInvalidUrisCases')]
    public function testInvalidUris(string $uri, string $message): void
    {
        $client = new MagicClient();
        try {
            $this->expectException(ClientRequestException::class);
            $this->expectExceptionMessage($message);
            $client->get($uri);
        } finally {
            $client->getClient()->close();
        }
    }

    /** @return iterable<string, array{string, string}> */
    public static function provideInvalidUrisCases(): iterable
    {
        yield 'relative URI' => ['/relative', 'absolute URI with host'];
        yield 'unsupported scheme' => ['ftp://example.test/', 'only supports http/https'];
    }
}
