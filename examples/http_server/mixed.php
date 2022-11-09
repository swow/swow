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

use Swow\Coroutine;
use Swow\CoroutineException;
use Swow\Errno;
use Swow\Http\Protocol\ProtocolException as HttpProtocolException;
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\UpgradeType;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket\Opcode as WebSocketOpcode;
use Swow\WebSocket\WebSocket;

require __DIR__ . '/../autoload.php';

ini_set('memory_limit', '1G');

$host = getenv('SERVER_HOST') ?: '127.0.0.1';
$port = (int) ((($port = getenv('SERVER_PORT')) !== '' && $port !== false) ? $port : 9764);
$backlog = (int) (getenv('SERVER_BACKLOG') ?: Socket::DEFAULT_BACKLOG);

$server = new Server();
$server->bind($host, $port)->listen($backlog);
while (true) {
    try {
        $connection = null;
        $connection = $server->acceptConnection();
        Coroutine::run(static function () use ($connection): void {
            try {
                while (true) {
                    $request = null;
                    try {
                        $request = $connection->recvHttpRequest();
                        switch ($request->getUri()->getPath()) {
                            case '/':
                                $connection->respond();
                                break;
                            case '/greeter':
                                $connection->respond(sprintf('Hello %s', Swow::class));
                                break;
                            case '/echo':
                                $connection->respond($request->getBody());
                                break;
                            case '/echo_all':
                                $connection->respond(Psr7::stringifyRequest($request));
                                break;
                            case '/httpbin':
                                /* proxy request to httpbin */
                                $connection->sendHttpResponse(
                                    ($httpBin ??= new Client())
                                        ->connect('httpbin.org', 80)
                                        ->sendRequest(
                                            $request->withUri(Psr7::createUriFromString(
                                                'http://httpbin.org/anything'
                                            ), preserveHost: false)
                                        )
                                );
                                break;
                            case '/chat':
                                $upgradeType = Psr7::detectUpgradeType($request);
                                if (!$upgradeType) {
                                    static $chatHtml;
                                    $connection->respond($chatHtml ??= file_get_contents(__DIR__ . '/chat.html'));
                                    break;
                                }
                                if (($upgradeType & UpgradeType::UPGRADE_TYPE_WEBSOCKET) === 0) {
                                    throw new HttpProtocolException(HttpStatus::BAD_REQUEST, 'Unsupported Upgrade Type');
                                }
                                $connection->upgradeToWebSocket($request);
                                $request = null;
                                while (true) {
                                    $frame = $connection->recvWebSocketFrame();
                                    $opcode = $frame->getOpcode();
                                    switch ($opcode) {
                                        case WebSocketOpcode::PING:
                                            $connection->send(WebSocket::PONG_FRAME);
                                            break;
                                        case WebSocketOpcode::PONG:
                                            break;
                                        case WebSocketOpcode::CLOSE:
                                            break 4;
                                        default:
                                            $connection->sendWebSocketFrame(
                                                Psr7::createWebSocketTextFrame(
                                                    payloadData: "You said: {$frame->getPayloadData()}"
                                                )
                                            );
                                    }
                                }
                            default:
                                $connection->error(HttpStatus::NOT_FOUND);
                        }
                    } catch (HttpProtocolException $exception) {
                        $connection->error($exception->getCode(), $exception->getMessage(), close: true);
                        break;
                    }
                    if (!$connection->shouldKeepAlive()) {
                        break;
                    }
                }
            } catch (Exception) {
                // you can log error here
            } finally {
                $connection->close();
            }
        });
    } catch (SocketException|CoroutineException $exception) {
        if (in_array($exception->getCode(), [Errno::EMFILE, Errno::ENFILE, Errno::ENOMEM], true)) {
            sleep(1);
        } else {
            break;
        }
    }
}
