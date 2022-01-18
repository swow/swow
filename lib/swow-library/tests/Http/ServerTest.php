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
use Swow\Coroutine;
use Swow\Http\Client as HttpClient;
use Swow\Http\Request as HttpRequest;
use Swow\Http\WebSocketFrame;
use function file_exists;
use function Swow\defer;

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
            $this->markTestSkipped('aaa');
        }
        $serverCoroutine = Coroutine::run(function () use ($mixedServerFile): void {
            require $mixedServerFile;
        });
        // so hacky ^^
        /* @var Server $server */
        $serverCoroutine->eval('$GLOBALS[\'server\'] = $this;');
        $server = $GLOBALS['server'];
        defer(function () use ($server): void {
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
        /* chat */
        for ($n = 1; $n <= 3; $n++) {
            $message = new WebSocketFrame();
            $message->getPayloadData()->write("Hello Swow {$n}");
            $reply = $client
                ->sendWebSocketFrame($message)
                ->recvWebSocketFrame();
            $this->assertStringContainsString("Hello Swow {$n}", $reply->getPayloadDataAsString());
        }
    }
}
