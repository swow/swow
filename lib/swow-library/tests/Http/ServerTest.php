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

namespace SwowTest\Http;

use PHPUnit\Framework\TestCase;
use Swow\Channel;
use Swow\Coroutine;
use Swow\Http\Client as HttpClient;
use Swow\Http\MimeType;
use Swow\Http\Request as HttpRequest;
use Swow\Http\Server as HttpServer;
use Swow\Http\Uri;
use Swow\Http\WebSocketFrame;
use Swow\Sync\WaitReference;
use Swow\WebSocket\Opcode;
use function file_exists;
use function getRandomBytes;
use function http_build_query;
use function json_encode;
use function serialize;
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

        $server = new HttpServer();
        $server->bind('127.0.0.1')->listen();
        $wr = new WaitReference();
        Coroutine::run(function () use ($server, $query, $wr): void {
            $connection = $server->acceptConnection();
            $request = $connection->recvHttpRequest();
            $this->assertSame($query, $request->getQueryParams());
            $this->assertSame($request->getHeaderLine('content-type'), MimeType::JSON);
            self::assertSame($request->getCookieParams(), ['foo' => 'bar', 'bar' => 'baz']);
            $this->assertSame($request->getServerParams()['remote_addr'], $request->getParsedBody()['address']);
            $this->assertSame($request->getServerParams()['remote_port'], $request->getParsedBody()['port']);
            $connection->respond(serialize($query));
        });

        $client = new HttpClient();
        $request = new HttpRequest();
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
        $this->assertSame($query, unserialize($response->getBodyAsString()));

        $wr::wait($wr);
    }

    public function testMixedServer(): void
    {
        $mixedServerFile = __DIR__ . '/../../../../examples/http_server/mixed.php';
        if (!file_exists($mixedServerFile)) {
            $this->markTestSkipped('unable to find mixed server example file');
        }
        $serverCoroutine = Coroutine::run(function () use ($mixedServerFile): void {
            require $mixedServerFile;
        });
        // so hacky ^^
        /** @var HttpServer $server */
        $server = $serverCoroutine->eval('$this');
        defer(static function () use ($server): void {
            $server->close();
        });

        /* HTTP */
        $client = new HttpClient();
        $request = new HttpRequest();
        $request
            ->setUriString('/echo')
            ->getBody()
            ->write('Hello Swow');
        $response = $client
            ->connect($server->getSockAddress(), $server->getSockPort())
            ->sendRequest($request);
        $this->assertSame('Hello Swow', $response->getBodyAsString());

        /* WebSocket */
        $request = new HttpRequest('GET', '/chat');
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
                    $client->sendString(WebSocketFrame::PING);
                    $heartBeatCount++;
                    usleep(1000);
                }
            });
        /* chat */
        for ($n = 1; $n <= TEST_MAX_REQUESTS; $n++) {
            $message = new WebSocketFrame();
            $message->getPayloadData()->write("Hello Swow {$n}");
            $client->sendWebSocketFrame($message);
            $reply = $messageChannel->pop($client->getReadTimeout());
            $this->assertStringContainsString("Hello Swow {$n}", $reply->getPayloadDataAsString());
        }
        $heartBeater->kill();
        $worker->kill();
        $this->assertGreaterThan(0, $heartBeatCount);
    }
}
