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

use PHPUnit\Framework\TestCase;
use Psr\Http\Message\UploadedFileInterface;
use ReflectionProperty;
use RuntimeException;
use Swow\Channel;
use Swow\Coroutine;
use Swow\Http\Http;
use Swow\Http\Message\HttpException;
use Swow\Http\Mime\MimeType;
use Swow\Http\Protocol\ReceiverTrait;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\Request;
use Swow\Psr7\Message\Uri;
use Swow\Psr7\Message\WebSocketFrame;
use Swow\Psr7\Server\Server;
use Swow\Socket;
use Swow\Sync\WaitReference;
use Swow\WebSocket\Opcode;
use Swow\WebSocket\WebSocket;

use function file_exists;
use function getRandomBytes;
use function http_build_query;
use function json_encode;
use function serialize;
use function sprintf;
use function str_repeat;
use function strlen;
use function Swow\defer;
use function unserialize;
use function usleep;

use const TEST_MAX_REQUESTS;

/**
 * @internal
 * @coversNothing
 */
final class ServerTest extends TestCase
{
    public function testHttpServer(): void
    {
        $randomBytes = getRandomBytes();
        $query = ['foo' => 'bar', 'baz' => $randomBytes];

        $server = new Server();
        $server->bind('127.0.0.1')->listen();
        $wr = new WaitReference();
        Coroutine::run(function () use ($server, $query, $wr): void {
            $connection = $server->acceptConnection();
            $request = $connection->recvHttpRequest();
            $this->assertSame($query, $request->getQueryParams());
            $this->assertSame($request->getHeaderLine('content-type'), MimeType::JSON);
            $this->assertSame($request->getCookieParams(), ['foo' => 'bar', 'bar' => 'baz']);
            $this->assertSame($request->getServerParams()['remote_addr'], $request->getParsedBody()['address']);
            $this->assertSame($request->getServerParams()['remote_port'], $request->getParsedBody()['port']);
            $connection->respond(serialize($query));
        });

        $client = new Client();
        $request = new Request();
        $uri = (new Uri('/'))->setQuery(http_build_query($query));
        $client
            ->connect($server->getSockAddress(), $server->getSockPort());
        $request
            ->setUri($uri)
            ->setHeader('Content-Type', MimeType::JSON)
            ->setHeader('Cookie', 'foo=bar; bar=baz')
            ->getBody()
            ->write(json_encode(['address' => $client->getSockAddress(), 'port' => $client->getSockPort()]));
        $response = $client->sendRequest($request);
        $this->assertSame($query, unserialize((string) $response->getBody()));

        $wr::wait($wr);
    }

    public function getMixedServer(): Server
    {
        $mixedServerFile = __DIR__ . '/../../../../../examples/http_server/mixed.php';
        if (!file_exists($mixedServerFile)) {
            $this->markTestSkipped('unable to find mixed server example file');
        }
        $serverCoroutine = Coroutine::run(function () use ($mixedServerFile): void {
            require $mixedServerFile;
        });
        // so hacky ^^
        /** @var Server $server */
        $server = $serverCoroutine->eval('$this');
        if (!$server instanceof Server) {
            throw new RuntimeException(sprintf('$server expect type of %s, got %s', Server::class, $server::class));
        }

        return $server;
    }

    public function testMixedServer(): void
    {
        $server = $this->getMixedServer();
        defer(static function () use ($server): void {
            $server->close();
        });
        /* HTTP */
        $client = new Client();
        $request = new Request();
        $request
            ->setUri('/echo')
            ->getBody()
            ->write('Hello Swow');
        $response = $client
            ->connect($server->getSockAddress(), $server->getSockPort())
            ->sendRequest($request);
        $this->assertSame('Hello Swow', (string) $response->getBody());

        /* WebSocket */
        $request = (new Request())->setMethod('GET')->setUri('/chat');
        $client->upgradeToWebSocket($request);
        $messageChannel = new Channel();
        $worker = Coroutine::run(
            /** @return never */
            static function () use ($client, $messageChannel): void {
                /* @phpstan-ignore-next-line */
                while (true) {
                    $message = $client->recvWebSocketFrame();
                    if ($message->getOpcode() === Opcode::TEXT) {
                        $messageChannel->push($message);
                    }
                }
            });
        $heartBeatCount = 0;
        $heartBeater = Coroutine::run(
            /** @return never */
            static function () use ($client, &$heartBeatCount): void {
                /* @phpstan-ignore-next-line */
                while (true) {
                    $client->send(WebSocket::PING_FRAME);
                    $heartBeatCount++;
                    usleep(1000);
                }
            });
        /* chat */
        for ($n = 1; $n <= TEST_MAX_REQUESTS; $n++) {
            $message = new WebSocketFrame();
            $message->getPayloadData()->write("Hello Swow {$n}");
            $client->sendWebSocketFrame($message);
            /** @var WebSocketFrame $reply */
            $reply = $messageChannel->pop($client->getReadTimeout());
            $this->assertStringContainsString("Hello Swow {$n}", (string) $reply->getPayloadData());
        }
        $heartBeater->kill();
        $worker->kill();
        $this->assertGreaterThan(0, $heartBeatCount);
    }

    public function testUploadFileWithCR(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $boundary = getRandomBytes(24);
        $headFormat =
            "POST /echo_all HTTP/1.1\r\n" .
            "Accept: */*\r\n" .
            "Host: {$server->getSockAddress()}:{$server->getSockPort()}\r\n" .
            "Connection: keep-alive\r\n" .
            "Content-Type: multipart/form-data; boundary=--------------------------{$boundary}\r\n" .
            "Content-Length: %d\r\n" .
            "\r\n";
        $bodyFormat =
            "----------------------------{$boundary}\r\n" .
            "Content-Disposition: form-data; name=\"img\"; filename=\"img.jpg\"\r\n" .
            "Content-Type: image/jpeg\r\n" .
            "\r\n" .
            '%s' .
            "\r\n" .
            "----------------------------{$boundary}--\r\n";

        $maxBufferSize = (new ReflectionProperty(ReceiverTrait::class, 'maxBufferSize'))->getDefaultValue();
        $fileData = str_repeat('0', 2 * $maxBufferSize);
        $body = sprintf($bodyFormat, $fileData);
        $request = sprintf($headFormat, strlen($body)) . $body;
        $request[2 * $maxBufferSize - 1] = "\r";

        $wr = new WaitReference();
        $channel = new Channel();
        Coroutine::run(static function () use ($server, $channel, $wr): void {
            $connection = $server->acceptConnection();
            $request = $connection->recvHttpRequest();
            $channel->push($request->getUploadedFiles());
            $connection->respond(Status::OK);
        });

        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $client->send($request);
        /** @var array<string, UploadedFileInterface> $uploadedFiles */
        $uploadedFiles = $channel->pop();
        $this->assertCount(1, $uploadedFiles);
        foreach ($uploadedFiles as $uploadedFile) {
            $this->assertSame(strlen($fileData), $uploadedFile->getSize());
            break;
        }
        $response = $client->recvResponseEntity();
        $this->assertSame(Status::OK, $response->statusCode);

        $wr::wait($wr);
    }

    public function testRequestUriOrHeaderFieldsTooLarge(): void
    {
        $maxBufferSize = (new ReflectionProperty(ReceiverTrait::class, 'maxBufferSize'))->getDefaultValue();

        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        $channel = new Channel();
        Coroutine::run(static function () use ($server, $channel, $wr): void {
            for ($i = 2; $i--;) {
                $connection = $server->acceptConnection();
                try {
                    $connection->recvHttpRequest();
                    $channel->push(null);
                } catch (HttpException $responseException) {
                    $channel->push($responseException->getCode());
                } finally {
                    $connection->close();
                }
            }
        });

        $client = new Socket(Socket::TYPE_TCP);
        $client
            ->connect($server->getSockAddress(), $server->getSockPort())
            ->send(Http::packRequest('GET', '/' . str_repeat('x', $maxBufferSize + 1)));
        $this->assertSame(Status::REQUEST_URI_TOO_LARGE, $channel->pop());

        $client = new Socket(Socket::TYPE_TCP);
        $client
            ->connect($server->getSockAddress(), $server->getSockPort())
            ->send(Http::packRequest('GET', '/', [
                'foo' => str_repeat('x', $maxBufferSize),
            ]));
        $this->assertSame(Status::REQUEST_HEADER_FIELDS_TOO_LARGE, $channel->pop());

        $wr::wait($wr);
    }
}
