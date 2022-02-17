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
use Swow\Http\Request as HttpRequest;
use Swow\Http\WebSocketFrame;
use Swow\WebSocket\Opcode;
use function file_exists;
use function Swow\defer;
use function usleep;
use const TEST_MAX_REQUESTS;

/**
 * @internal
 * @coversNothing
 */
final class ServerTest extends TestCase
{
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
        /* @var Server $server */
        $serverCoroutine->eval('$GLOBALS[\'server\'] = $this;');
        $server = $GLOBALS['server'];
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
        $worker = Coroutine::run(static function () use ($client, $messageChannel): void {
            while (true) {
                $message = $client->recvWebSocketFrame();
                if ($message->getOpcode() === Opcode::TEXT) {
                    $messageChannel->push($message);
                }
            }
        });
        $heartBeatCount = 0;
        $heartBeater = Coroutine::run(static function () use ($client, &$heartBeatCount): void {
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
