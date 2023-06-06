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
use Swow\Errno;
use Swow\Http\Http;
use Swow\Http\Mime\MimeType;
use Swow\Http\Protocol\ProtocolException as HttpProtocolException;
use Swow\Http\Protocol\ReceiverTrait;
use Swow\Http\Status;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\Request;
use Swow\Psr7\Message\UpgradeType;
use Swow\Psr7\Message\WebSocketFrame;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;
use Swow\TestUtils\Testing;
use Swow\WebSocket\Opcode;
use Swow\WebSocket\WebSocket;

use function file_exists;
use function http_build_query;
use function json_encode;
use function microtime;
use function msleep;
use function putenv;
use function serialize;
use function sprintf;
use function str_repeat;
use function strlen;
use function substr;
use function Swow\defer;
use function Swow\TestUtils\getRandomBytes;
use function Swow\TestUtils\pseudoRandom;
use function unserialize;
use function usleep;

/**
 * @internal
 * @covers \Swow\Psr7\Server\Server
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
        $client
            ->connect($server->getSockAddress(), $server->getSockPort());
        $request = Psr7::createRequest('GET', '/?' . http_build_query($query), [
            'Content-Type' => MimeType::JSON,
            'Cookie' => 'foo=bar; bar=baz',
        ], Psr7::createStream(json_encode([
            'address' => $client->getSockAddress(),
            'port' => $client->getSockPort(),
        ])));
        $response = $client->sendRequest($request);
        $this->assertSame($query, unserialize((string) $response->getBody()));

        $wr::wait($wr);
    }

    public function getMixedServer(): Server
    {
        putenv('SERVER_PORT=0');
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
        $request = Psr7::createRequest(
            method: 'POST',
            uri: '/echo',
            body: Psr7::createStream('Hello Swow')
        );
        $response = $client
            ->connect($server->getSockAddress(), $server->getSockPort())
            ->sendRequest($request);
        $this->assertSame('Hello Swow', (string) $response->getBody());

        /* WebSocket */
        $request = Psr7::createRequest(method: 'GET', uri: '/chat');
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
        for ($n = 1; $n <= Testing::$maxRequests; $n++) {
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

    public function testRecvRequestTimeout(): void
    {
        $wr = new WaitReference();
        foreach ([
            ['server' => 500, 'connection' => -1],
            ['server' => -1, 'connection' => 500],
        ] as $recvMessageTimeout) {
            foreach ([0, 100, 1000, 10 * 1000, -1] as $readTimeout) {
                Coroutine::run(function () use ($recvMessageTimeout, $readTimeout, $wr): void {
                    $server = new Server();
                    $server->setRecvMessageTimeout($recvMessageTimeout['server']);
                    $server->bind('127.0.0.1')->listen();
                    $attacker = Coroutine::run(function () use ($server): void {
                        $client = new Socket(Socket::TYPE_TCP);
                        $client->connect($server->getSockAddress(), $server->getSockPort());
                        $requestText = "GET / HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 1000\r\n\r\n" . str_repeat('X', 1000);
                        $client->send($requestText);
                        $this->assertSame('X', $client->readString(1));
                        for ($i = 0; $i < strlen($requestText); $i++) {
                            $client->send($requestText[$i]);
                            msleep(100);
                        }
                    });
                    defer(static function () use ($attacker): void {
                        $attacker->isExecuting() && $attacker->kill();
                    });
                    $connection = $server->acceptConnection();
                    $connection->setReadTimeout($readTimeout);

                    $request = $connection->recvHttpRequest($recvMessageTimeout['connection']);

                    $connection->send('X');
                    $this->assertSame($request->getHeaderLine('Connection'), 'keep-alive');
                    $exception = null;
                    $s = microtime(true);
                    try {
                        $connection->recvHttpRequest(500);
                    } catch (SocketException $exception) {
                    }
                    $this->assertInstanceOf(SocketException::class, $exception);
                    $this->assertSame(Errno::ETIMEDOUT, $exception->getCode());
                    $d = ((microtime(true) - $s) * 1000) + 1;
                    if ($readTimeout >= 500) {
                        $this->assertGreaterThanOrEqual(500, $d);
                    }
                });
            }
        }
        $wr::wait($wr);
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
                } catch (HttpProtocolException $protocolException) {
                    $channel->push($protocolException->getCode());
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

    public function testBroadcastWebSocketFrame(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();
        $random = getRandomBytes();
        $upgradedWebSocketCount = 0;
        $wr = new WaitReference();
        for ($c = 0; $c < Testing::$maxConcurrencyMid; $c++) {
            Coroutine::run(function () use ($server, $random, &$upgradedWebSocketCount, $wr): void {
                $client = new Client();
                $client->connect($server->getSockAddress(), $server->getSockPort());
                if (pseudoRandom() % 2) {
                    $request = Psr7::createRequest(method: 'GET', uri: '/');
                    $response = $client->upgradeToWebSocket($request);
                    $upgradedWebSocketCount++;
                    $this->assertSame(Status::SWITCHING_PROTOCOLS, $response->getStatusCode());
                    for ($n = 0; $n < 2; $n++) {
                        $frame = $client->recvWebSocketFrame();
                        $this->assertSame($random, (string) $frame->getPayloadData());
                    }
                } else {
                    $request = Psr7::createRequest(
                        method: 'POST',
                        uri: '/',
                        body: Psr7::createStream($random)
                    );
                    $response = $client->sendRequest($request);
                    $this->assertSame($random, (string) $response->getBody());
                }
            });
        }
        $wrUpgrade = new WaitReference();
        $connections = [];
        $acceptedWebSocketCount = 0;
        for ($c = 0; $c < Testing::$maxConcurrencyMid; $c++) {
            $connections[] = $connection = $server->acceptConnection();
            Coroutine::run(static function () use ($connection, &$acceptedWebSocketCount, $wrUpgrade): void {
                $request = $connection->recvHttpRequest();
                if (Psr7::detectUpgradeType($request) === UpgradeType::UPGRADE_TYPE_WEBSOCKET) {
                    $connection->upgradeToWebSocket($request);
                    $acceptedWebSocketCount++;
                } else {
                    $connection->respond($request->getBody());
                }
            });
        }
        $wrUpgrade::wait($wrUpgrade);
        $result1 = $server->broadcastWebSocketFrame(frame: Psr7::createWebSocketTextFrame($random));
        $result2 = $server->broadcastWebSocketFrame(frame: Psr7::createWebSocketTextFrame($random), targets: $connections);
        $wr::wait($wr);
        $this->assertSame($acceptedWebSocketCount, $upgradedWebSocketCount);
        $this->assertSame($acceptedWebSocketCount, $result1->getCount());
        $this->assertSame($acceptedWebSocketCount, $result1->getSuccessCount());
        $this->assertSame(0, $result1->getFailureCount());
        $this->assertNull($result1->getExceptions());
        $this->assertSame($acceptedWebSocketCount, $result2->getCount());
        $this->assertSame($acceptedWebSocketCount, $result2->getSuccessCount());
        $this->assertSame(0, $result2->getFailureCount());
        $this->assertNull($result2->getExceptions());
    }

    public function testEventNone(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->acceptConnection();
            $connection->recvHttpRequest();
            $connection->respond(Status::OK);
        });

        $request = Psr7::createRequest(
            'GET', '/test', [
                'Host' => "{$server->getSockAddress()}:{$server->getSockPort()}",
            ]
        );
        $request = Psr7::stringifyRequest($request);
        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $client->send(substr($request, 0, (int) (strlen($request) / 2)));
        usleep(1);
        $client->send(substr($request, (int) (strlen($request) / 2)));
        $response = $client->recvResponseEntity();
        $this->assertSame(Status::OK, $response->statusCode);

        $wr::wait($wr);
    }

    public function testWebSocketUnmask(): void
    {
        $server = $this->getMixedServer();
        defer(static function () use ($server): void {
            $server->close();
        });
        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $request = Psr7::createRequest(method: 'GET', uri: '/chat');
        $client->upgradeToWebSocket($request);
        for ($n = 0; $n < Testing::$maxRequests; $n++) {
            $random = getRandomBytes();
            $sentFrame = Psr7::createWebSocketTextMaskedFrame($random);
            $this->assertTrue($sentFrame->getMask());
            $this->assertNotEmpty($sentFrame->getMaskingKey());
            $this->assertNotSame((string) $sentFrame->getPayloadData(), $random);
            $client->sendWebSocketFrame($sentFrame);
            $receivedFrame = $client->recvWebSocketFrame();
            $this->assertFalse($receivedFrame->getMask());
            $this->assertEmpty($receivedFrame->getMaskingKey());
            $this->assertSame(substr((string) $receivedFrame->getPayloadData(), -strlen($random)), $random);
        }
    }

    public function testWebSocketBigData(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();
        $random = str_repeat(getRandomBytes(1024), 1024);
        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $random, $wr): void {
            $connection = $server->acceptConnection();
            $connection->upgradeToWebSocket($connection->recvHttpRequest());
            $frame = $connection->recvWebSocketFrame();
            $connection->sendWebSocketFrame($frame);
        });
        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $request = Psr7::createRequest(method: 'GET', uri: '/chat');
        $client->upgradeToWebSocket($request);
        $client->sendWebSocketFrame(Psr7::createWebSocketTextFrame($random));
        $frame = $client->recvWebSocketFrame();
        $this->assertSame((string) $frame->getPayloadData(), $random);
        $wr::wait($wr);
    }

    public function testEmptyContentType(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();
        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->acceptConnection();
            $request = $connection->recvHttpRequest();
            $connection->respond(serialize($request));
        });
        $client = new Client();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $request = Psr7::createRequest(method: 'GET', uri: '/', headers: ['Content-Type' => '']);
        $response = $client->sendRequest($request);
        $wr::wait($wr);
        /** @var Request $requestDup */
        $requestDup = unserialize((string) $response->getBody());
        $this->assertTrue($requestDup->hasHeader('Content-Type'));
        $this->assertSame('', $requestDup->getHeaderLine('Content-Type'));
    }
}
