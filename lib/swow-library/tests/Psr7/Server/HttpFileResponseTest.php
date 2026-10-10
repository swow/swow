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

use InvalidArgumentException;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Http\Http;
use Swow\Http\Mime\MimeType;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;

use function strlen;

/** @internal */
#[CoversClass(ServerConnection::class)]
#[CoversClass(Server::class)]
#[CoversClass(Client::class)]
#[CoversClass(Psr7::class)]
#[CoversClass(MimeType::class)]
final class HttpFileResponseTest extends TestCase
{
    private Server $server;

    private Client $client;

    private ServerConnection $connection;

    private string $filename;

    private string $data = "0123456789\0binary file";

    protected function setUp(): void
    {
        $filename = tempnam(sys_get_temp_dir(), 'swow-http-file-');
        $this->assertIsString($filename);
        $this->filename = $filename;
        file_put_contents($this->filename, $this->data);
        $this->server = new Server();
        $this->server->setTimeout(1000)->bind('127.0.0.1')->listen();
        $this->client = new Client();
        $this->client->setTimeout(1000)->connect($this->server->getSockAddress(), $this->server->getSockPort());
        $this->connection = $this->server->acceptConnection(1000);
        $this->client->getSocket()->send(Http::packRequest('GET', '/file', ['Host' => 'example.test']));
        $this->connection->recvHttpRequest(1000);
    }

    protected function tearDown(): void
    {
        $this->connection->close();
        $this->client->close();
        $this->server->close();
        unlink($this->filename);
    }

    #[DataProvider('provideFileResponsesPreserveBinaryDataAndHeadersCases')]
    public function testFileResponsesPreserveBinaryDataAndHeaders(int $offset, int $length, ?string $type): void
    {
        $headers = ['X-File' => ['local']];
        if ($type !== null) {
            $headers['Content-Type'] = [$type];
        }
        $response = Psr7::createResponse(headers: $headers);
        $expected = $length <= 0 ? $this->data : substr($this->data, $offset, $length);
        $this->assertSame(strlen($expected), $this->connection->sendHttpFile($response, $this->filename, $offset, $length, 1000));
        $received = Psr7::createResponseFromEntity($this->client->recvResponseEntity(1000));
        $this->assertSame(Status::OK, $received->getStatusCode());
        $this->assertSame($expected, (string) $received->getBody());
        $this->assertSame((string) strlen($expected), $received->getHeaderLine('Content-Length'));
        $this->assertSame($type ?? MimeType::BIN, $received->getHeaderLine('Content-Type'));
        $this->assertSame('local', $received->getHeaderLine('X-File'));
        $this->assertFalse($response->hasHeader('Content-Length'));
    }

    /** @return iterable<string, array{int, int, ?string}> */
    public static function provideFileResponsesPreserveBinaryDataAndHeadersCases(): iterable
    {
        yield 'whole file' => [0, -1, null];
        yield 'zero means unlimited' => [0, 0, 'application/custom'];
        yield 'binary slice' => [8, 5, 'application/octet-stream'];
    }

    public function testCallerCannotDeclareAnUnrelatedContentLength(): void
    {
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Content-Length cannot be set');
        $this->connection->sendHttpFile(Psr7::createResponse(headers: ['content-length' => '1']), $this->filename);
    }
}
