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
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Client\Client;
use Swow\Psr7\Message\ResponseException;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\WebSocketFrame;
use Swow\Psr7\Server\Server;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket;
use Swow\WebSocket\Opcode as WebSocketOpcode;

require __DIR__ . '/../autoload.php';

ini_set('memory_limit', '1G');

$host = getenv('SERVER_HOST') ?: '127.0.0.1';
$port = (int) (getenv('SERVER_PORT') ?: 9764);
$backlog = (int) (getenv('SERVER_BACKLOG') ?: Socket::DEFAULT_BACKLOG);

$server = new Server();
$server->bind($host, $port)->listen($backlog);
while (true) {
    try {
        $connection = $server->acceptConnection();
        Coroutine::run(static function () use ($connection): void {
            try {
                while (true) {
                    $request = null;
                    try {
                        /** @var ServerRequest $request */
                        $request = $connection->recvHttpRequest();
                        switch ($request->getUri()->getPath()) {
                            case '/':
                                $connection->respond();
                                break;
                            case '/greeter':
                                $connection->respond('Hello Swow');
                                break;
                            case '/echo':
                                $connection->respond((string) $request->getBody());
                                break;
                            case '/echo_all':
                                $connection->respond($request->toString());
                                break;
                            case '/httpbin':
                                /* proxy request to httpbin */
                                $connection->sendHttpResponse(
                                    ($httpBin ??= new Client())
                                        ->connect('httpbin.org', 80)
                                        ->sendRequest(
                                            $request->withUri('http://httpbin.org/anything', false)
                                        )
                                );
                                break;
                            case '/chat':
                                if ($upgrade = $request->getUpgrade()) {
                                    if ($upgrade === $request::UPGRADE_WEBSOCKET) {
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
                                                    break 2;
                                                default:
                                                    $connection->sendWebSocketFrame(
                                                        new WebSocketFrame(payloadData: "You said: {$frame->getPayloadData()}")
                                                    );
                                            }
                                        }
                                        break;
                                    }
                                    throw new ResponseException(HttpStatus::BAD_REQUEST, 'Unsupported Upgrade Type');
                                }
                                $connection->respond(file_get_contents(__DIR__ . '/chat.html'));
                                break;
                            default:
                                $connection->error(HttpStatus::NOT_FOUND);
                        }
                    } catch (ResponseException $exception) {
                        $connection->error($exception->getCode(), $exception->getMessage());
                    }
                    if (!$request || !$request->shouldKeepAlive()) {
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
