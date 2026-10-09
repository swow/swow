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
use PHPUnit\Framework\TestCase;
use Psr\Http\Message\ResponseInterface;
use Swow\Coroutine;
use Swow\Http\Http;
use Swow\Http\Status;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\Socket;
use Swow\Sync\WaitReference;
use Swow\Utils\FileSystem\FileSystem;

use function array_map;
use function file_exists;
use function is_array;
use function is_numeric;
use function method_exists;
use function mkdir;
use function sprintf;
use function str_contains;
use function strcasecmp;
use function Swow\TestUtils\getRandomBytes;

use const CURLOPT_HEADER;
use const CURLOPT_PROXY;
use const CURLOPT_RETURNTRANSFER;
use const CURLOPT_URL;
use const PHP_VERSION_ID;

/**
 * @internal
 */
#[CoversClass(ServerConnection::class)]
final class ServerConnectionTest extends TestCase
{
    protected string $tempFile = '';

    protected string $tempDir = '';

    protected function setUp(): void
    {
        $tempDir = __DIR__ . '/temp';

        if (file_exists($tempDir)) {
            FileSystem::remove($tempDir);
        }
        mkdir($tempDir);

        $this->tempDir = $tempDir;

        $fileName = uniqid() . '.txt';
        $this->tempFile = $tempDir . '/' . $fileName;
        file_put_contents($this->tempFile, getRandomBytes(8192));
    }

    protected function tearDown(): void
    {
        FileSystem::remove($this->tempDir);
    }

    public function testSendHttpFile(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $tempFile = $this->tempFile;
        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $tempFile, $wr): void {
            $connection = $server->acceptConnection();
            $connection->recvHttpRequest();
            $response = Psr7::createResponse();
            $connection->sendHttpFile($response, $tempFile);
        });

        $url = sprintf('%s:%s', $server->getSockAddress(), $server->getSockPort());
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
        curl_setopt($ch, CURLOPT_HEADER, true);
        curl_setopt($ch, CURLOPT_PROXY, false);
        $response = curl_exec($ch);
        if (PHP_VERSION_ID < 80100) {
            curl_close($ch);
        }

        $fileSize = filesize($this->tempFile);
        [$headerLines, $body] = explode("\r\n\r\n", $response);
        // parse headers to array
        $headerLines = explode("\r\n", $headerLines);
        $headLine = array_shift($headerLines);
        $headLine = explode(' ', $headLine, 3);
        if (!isset($headLine[1]) || !is_numeric($headLine[1])) {
            $this->fail('Invalid response');
        }
        $status = (int) $headLine[1];
        $headers = [];
        foreach ($headerLines as $headerLine) {
            $headerLineParts = array_map('trim', explode(':', $headerLine, 2));
            $headers[$headerLineParts[0]] = $headerLineParts[1] ?? '';
        }
        $this->assertSame(Status::OK, $status);

        $this->assertSame('text/plain', $headers['Content-Type'] ?? '');
        $this->assertSame($fileSize, (int) ($headers['Content-Length'] ?? ''));
        $this->assertSame(file_get_contents($this->tempFile), $body);

        $wr::wait($wr);
        $server->close();
    }

    public function testSendEventStreamHeaders(): void
    {
        $this->assertTrue(method_exists(ServerConnection::class, 'sendEventStreamHeaders'));

        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->acceptConnection();
            $connection->recvHttpRequest();
            $connection->sendEventStreamHeaders();
            $connection->close();
        });

        $client = new Socket(Socket::TYPE_TCP);
        $client->setReadTimeout(1000);
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $client->send(Http::packRequest('GET', '/events', [
            'Host' => "{$server->getSockAddress()}:{$server->getSockPort()}",
            'Accept' => 'text/event-stream',
        ]));

        $response = '';
        while (!str_contains($response, "\r\n\r\n")) {
            $response .= $client->recvString(1024);
        }

        $this->assertStringContainsString("HTTP/1.1 200 OK\r\n", $response);
        $this->assertStringContainsString("Cache-Control: no-store\r\n", $response);
        $this->assertStringContainsString("Content-Type: text/event-stream\r\n", $response);
        $this->assertStringContainsString("Transfer-Encoding: chunked\r\n", $response);
        $this->assertStringNotContainsString('Content-Length:', $response);

        $wr::wait($wr);
        $server->close();
    }

    public function testSendPlainPsrResponseUpdatesKeepAlive(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(function () use ($server, $wr): void {
            $connection = $server->acceptConnection();
            $connection->recvHttpRequest();

            $headers = ['Connection' => ['close']];
            $response = $this->createMock(ResponseInterface::class);
            $response->method('getStatusCode')->willReturn(Status::OK);
            $response->method('getReasonPhrase')->willReturn('OK');
            $response->method('getProtocolVersion')->willReturn('1.1');
            $response->method('getBody')->willReturn(Psr7::createStream('pong'));
            $response->method('getHeaders')->willReturnCallback(static function () use (&$headers): array {
                return $headers;
            });
            $response->method('hasHeader')->willReturnCallback(static function (string $name) use (&$headers): bool {
                foreach ($headers as $headerName => $_) {
                    if (strcasecmp($headerName, $name) === 0) {
                        return true;
                    }
                }
                return false;
            });
            $response->method('getHeaderLine')->willReturnCallback(static function (string $name) use (&$headers): string {
                foreach ($headers as $headerName => $headerValue) {
                    if (strcasecmp($headerName, $name) === 0) {
                        return implode(', ', $headerValue);
                    }
                }
                return '';
            });
            $response->method('withHeader')->willReturnCallback(static function (string $name, mixed $value) use (&$headers, $response): ResponseInterface {
                $headers[$name] = is_array($value) ? $value : [(string) $value];
                return $response;
            });

            $connection->sendHttpResponse($response);
            $this->assertFalse($connection->shouldKeepAlive());
            $connection->close();
        });

        $client = new Socket(Socket::TYPE_TCP);
        $client->setReadTimeout(1000);
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $client->send(Http::packRequest('GET', '/', [
            'Host' => "{$server->getSockAddress()}:{$server->getSockPort()}",
            'Connection' => 'keep-alive',
        ]));

        $response = '';
        while (!str_contains($response, 'pong')) {
            $response .= $client->recvString(1024);
        }

        $this->assertStringContainsString("Connection: close\r\n", $response);
        $this->assertStringContainsString("Content-Length: 4\r\n", $response);

        $wr::wait($wr);
        $server->close();
    }
}
