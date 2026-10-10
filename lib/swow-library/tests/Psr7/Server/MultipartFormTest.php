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
use Swow\Psr7\Message\UploadedFile;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;

use function strlen;

use const UPLOAD_ERR_OK;

/** @internal */
#[CoversClass(Server::class)]
#[CoversClass(ServerConnection::class)]
#[CoversClass(ServerRequest::class)]
#[CoversClass(UploadedFile::class)]
#[CoversClass(Psr7::class)]
#[CoversClass(Client::class)]
final class MultipartFormTest extends TestCase
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

    #[DataProvider('provideFieldsAndLargeBinaryUploadsCases')]
    public function testFieldsAndLargeBinaryUploads(bool $preserveBody): void
    {
        $this->connection->setPreserveBodyData($preserveBody);
        $this->assertSame($preserveBody, $this->connection->isPreserveBodyData());
        $data = str_repeat("binary\0body", Buffer::COMMON_SIZE);
        $body = "--swow-form\r\nContent-Disposition: form-data; name=\"title\"\r\n\r\nSwow\r\n"
            . "--swow-form\r\nContent-Disposition: form-data; name=\"empty\"\r\n\r\n\r\n"
            . "--swow-form\r\nContent-Disposition: form-data; name=\"upload\"; filename=\"data.bin\"\r\n"
            . "Content-Type: application/octet-stream\r\n\r\n{$data}\r\n"
            . "--swow-form\r\nContent-Disposition: form-data; name=\"empty-upload\"; filename=\"empty.bin\"\r\n"
            . "Content-Type: application/octet-stream\r\n\r\n\r\n--swow-form--\r\n";
        $wire = Http::packRequest('POST', '/upload', [
            'Host' => 'example.test',
            'Content-Type' => 'multipart/form-data; boundary=swow-form',
            'Content-Length' => strlen($body),
        ], $body);
        $writer = Coroutine::run(function () use ($wire): void {
            $this->client->getSocket()->send($wire);
        });
        $uploads = [];
        try {
            $request = $this->connection->recvHttpRequest();
            $this->assertSame(['title' => 'Swow', 'empty' => ''], $request->getParsedBody());
            $this->assertSame($preserveBody ? $body : '', (string) $request->getBody());
            $this->assertSame((string) strlen($body), $request->getHeaderLine('Content-Length'));
            $uploads = $request->getUploadedFiles();
            $this->assertCount(2, $uploads);
            $upload = $uploads['upload'];
            $this->assertSame('data.bin', $upload->getClientFilename());
            $this->assertSame('application/octet-stream', $upload->getClientMediaType());
            $this->assertSame(UPLOAD_ERR_OK, $upload->getError());
            $this->assertSame(strlen($data), $upload->getSize());
            $this->assertSame($data, (string) $upload->getStream());
            $empty = $uploads['empty-upload'];
            $this->assertSame('empty.bin', $empty->getClientFilename());
            $this->assertSame(UPLOAD_ERR_OK, $empty->getError());
            $this->assertSame(0, $empty->getSize());
            $this->assertSame('', (string) $empty->getStream());
            $this->assertNotSame($upload->toArray()['tmp_file'], $empty->toArray()['tmp_file']);
        } finally {
            if ($writer->isAlive()) {
                $writer->kill();
            }
            foreach ($uploads as $file) {
                $filename = $file->toArray()['tmp_file'];
                $file->getStream()->close();
                if (file_exists($filename)) {
                    unlink($filename);
                }
            }
        }
    }

    /** @return iterable<string, array{bool}> */
    public static function provideFieldsAndLargeBinaryUploadsCases(): iterable
    {
        yield 'parsed fields and uploads only' => [false];
        yield 'retain wire body' => [true];
    }

    #[DataProvider('provideInvalidContentDispositionsAreRejectedCases')]
    public function testInvalidContentDispositionsAreRejected(string $disposition, string $message): void
    {
        $body = "--swow-form\r\nContent-Disposition: {$disposition}\r\n\r\nvalue\r\n--swow-form--\r\n";
        $this->client->getSocket()->send(Http::packRequest('POST', '/upload', [
            'Host' => 'example.test',
            'Content-Type' => 'multipart/form-data; boundary=swow-form',
            'Content-Length' => strlen($body),
        ], $body));
        $this->expectException(ProtocolException::class);
        $this->expectExceptionCode(Status::BAD_REQUEST);
        $this->expectExceptionMessage($message);
        $this->connection->recvHttpRequest();
    }

    /** @return iterable<string, array{string, string}> */
    public static function provideInvalidContentDispositionsAreRejectedCases(): iterable
    {
        yield 'unsupported type' => ['unsupported; name="field"', 'Unsupported Content-Disposition type'];
        yield 'missing field name' => ['form-data', 'Missing name or filename in Content-Disposition'];
    }
}
