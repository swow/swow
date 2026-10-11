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
use Swow\Channel;
use Swow\Coroutine;
use Swow\Http\Http;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\UploadedFile;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\Socket;
use Swow\SocketException;

use function count;
use function is_string;
use function strlen;

use const UPLOAD_ERR_OK;

/** @internal */
#[CoversClass(Server::class)]
#[CoversClass(ServerConnection::class)]
#[CoversClass(ServerRequest::class)]
#[CoversClass(UploadedFile::class)]
#[CoversClass(Psr7::class)]
#[CoversClass(Client::class)]
final class MultipartRequestTest extends TestCase
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

    #[DataProvider('provideMultipartFieldsAndUploadsCases')]
    public function testMultipartFieldsAndUploads(bool $preserveBody, string $epilogue): void
    {
        $this->connection->setPreserveBodyData($preserveBody);
        $this->assertSame($preserveBody, $this->connection->isPreserveBodyData());
        $body = "--swow-boundary\r\nContent-Disposition: form-data; name=\"title\"\r\n\r\nSwow\r\n"
            . "--swow-boundary\r\nContent-Disposition: form-data; name=\"upload\"; filename=\"data.bin\"\r\n"
            . "Content-Type: application/octet-stream\r\n\r\na\0b\r\n--swow-boundary--\r\n" . $epilogue;
        $this->client->getSocket()->send(Http::packRequest('POST', '/upload', [
            'Host' => 'example.test',
            'Content-Type' => 'multipart/form-data; boundary=swow-boundary',
            'Content-Length' => strlen($body),
        ], $body) . Http::packRequest('GET', '/next', ['Host' => 'example.test']));
        $upload = null;
        $filename = null;
        try {
            $request = $this->connection->recvHttpRequest();
            $this->assertSame(['title' => 'Swow'], $request->getParsedBody());
            $this->assertSame($preserveBody ? $body : '', (string) $request->getBody());
            $this->assertCount(1, $request->getUploadedFiles());
            $upload = $request->getUploadedFiles()['upload'];
            $filename = $upload->toArray()['tmp_file'];
            $this->assertSame('data.bin', $upload->getClientFilename());
            $this->assertSame('application/octet-stream', $upload->getClientMediaType());
            $this->assertSame(UPLOAD_ERR_OK, $upload->getError());
            $this->assertSame(3, $upload->getSize());
            $this->assertSame("a\0b", (string) $upload->getStream());
            $this->assertSame('/next', $this->connection->recvHttpRequest()->getRequestTarget());
        } finally {
            $upload?->getStream()->close();
            if ($filename !== null && file_exists($filename)) {
                unlink($filename);
            }
        }
    }

    /** @return iterable<string, array{bool, string}> */
    public static function provideMultipartFieldsAndUploadsCases(): iterable
    {
        foreach ([false, true] as $preserveBody) {
            foreach (['', 'legal epilogue'] as $epilogue) {
                yield 'preserve ' . (int) $preserveBody . ' epilogue ' . strlen($epilogue) => [$preserveBody, $epilogue];
            }
        }
    }

    #[DataProvider('provideMultipartHeaderFragmentsAndTheNextRequestStaySeparateCases')]
    public function testMultipartHeaderFragmentsAndTheNextRequestStaySeparate(int $split): void
    {
        $body = "--form\r\nContent-Disposition: form-data; name=\"title\"\r\n\r\nvalue\r\n--form--\r\n";
        $head = Http::packRequest('POST', '/upload', [
            'Host' => 'example.test',
            'Content-Type' => 'multipart/form-data; boundary=form',
            'Content-Length' => strlen($body),
        ]);
        $this->client->getSocket()->send($head . substr($body, 0, $split));
        $result = new Channel(1);
        $reader = Coroutine::run(function () use ($result): void {
            $result->push($this->connection->recvHttpRequest());
        });
        try {
            $this->assertTrue($reader->isAlive());
            $this->client->getSocket()->send(substr($body, $split) . Http::packRequest('GET', '/next', ['Host' => 'example.test']));
            $this->assertSame(['title' => 'value'], $result->pop(1000)->getParsedBody());
            $this->assertSame('/next', $this->connection->recvHttpRequest()->getRequestTarget());
        } finally {
            if ($reader->isAlive()) {
                $reader->kill();
            }
        }
    }

    /** @return iterable<string, array{int}> */
    public static function provideMultipartHeaderFragmentsAndTheNextRequestStaySeparateCases(): iterable
    {
        yield 'part header name' => [strlen("--form\r\nContent-Dispo")];
        yield 'part header value' => [strlen("--form\r\nContent-Disposition: form-data; name=\"ti")];
    }

    public function testMultipartEofBeforeTheDeclaredBodyLengthIsRejected(): void
    {
        $body = "--form\r\nContent-Disposition: form-data; name=\"title\"\r\n\r\nvalue\r\n--form--";
        $this->client->getSocket()->send(Http::packRequest('POST', '/upload', [
            'Content-Type' => 'multipart/form-data; boundary=form',
            'Content-Length' => strlen($body) + 2,
        ], $body));
        $this->client->close();
        $this->expectException(SocketException::class);
        $this->connection->recvHttpRequest();
    }

    #[DataProvider('provideInterruptedUploadReleasesItsTemporaryFileCases')]
    public function testInterruptedUploadReleasesItsTemporaryFile(bool $completedPart): void
    {
        $body = $completedPart ? "--form\r\nContent-Disposition: form-data; name=\"completed\"; filename=\"completed.bin\"\r\n\r\ncomplete\r\n" : '';
        $body .= "--form\r\nContent-Disposition: form-data; name=\"upload\"; filename=\"file.bin\"\r\n"
            . "Content-Type: application/octet-stream\r\n\r\npartial";
        $before = get_resources('stream');
        $this->client->getSocket()->send(Http::packRequest('POST', '/upload', [
            'Content-Type' => 'multipart/form-data; boundary=form',
            'Content-Length' => strlen($body) + 100,
        ], $body));
        $result = new Channel(1);
        $reader = Coroutine::run(function () use ($result): void {
            try {
                $this->connection->recvHttpRequest();
                $result->push(false);
            } catch (SocketException) {
                $result->push(true);
            }
        });
        $paths = [];
        try {
            $deadline = Coroutine::getMain()->getElapsed() + 1000;
            do {
                $paths = [];
                foreach (array_diff_key(get_resources('stream'), $before) as $stream) {
                    $path = stream_get_meta_data($stream)['uri'] ?? null;
                    if (is_string($path) && str_starts_with(basename($path), 'swow_uploaded_file_')) {
                        $paths[] = $path;
                    }
                }
                unset($stream);
                // Observe file creation and the following socket wait, including asynchronous file I/O.
                if (count($paths) === ($completedPart ? 2 : 1) && $this->connection->getSocket()->getIoState() === Socket::IO_FLAG_READ) {
                    break;
                }
                usleep(1000);
            } while ($reader->isAlive() && Coroutine::getMain()->getElapsed() < $deadline);
            $this->assertCount($completedPart ? 2 : 1, $paths);
            foreach ($paths as $path) {
                $this->assertTrue(file_exists($path));
            }
            $this->client->close();
            $this->assertTrue($result->pop(1000));
            foreach ($paths as $path) {
                $this->assertFalse(file_exists($path));
            }
        } finally {
            if ($reader->isAlive()) {
                $reader->kill();
            }
            foreach ($paths as $path) {
                if (file_exists($path)) {
                    unlink($path);
                }
            }
        }
    }

    /** @return iterable<string, array{bool}> */
    public static function provideInterruptedUploadReleasesItsTemporaryFileCases(): iterable
    {
        yield 'active upload' => [false];
        yield 'completed part and active upload' => [true];
    }
}
