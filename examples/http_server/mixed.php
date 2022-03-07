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

ini_set('memory_limit', '1G');

require __DIR__ . '/../autoload.php';

use Swow\Coroutine;
use Swow\CoroutineException;
use Swow\Errno;
use Swow\Http\ResponseException;
use Swow\Http\Server as HttpServer;
use Swow\Http\Status as HttpStatus;
use Swow\Http\WebSocketFrame;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket\Opcode as WebSocketOpcode;

$host = getenv('SERVER_HOST') ?: '127.0.0.1';
$port = (int) (getenv('SERVER_PORT') ?: 9764);
$backlog = (int) (getenv('SERVER_BACKLOG') ?: Socket::DEFAULT_BACKLOG);

$server = new HttpServer();
$server->bind($host, $port)->listen($backlog);
while (true) {
    try {
        $connection = $server->acceptConnection();
        Coroutine::run(static function () use ($connection): void {
            try {
                while (true) {
                    $request = null;
                    try {
                        $request = $connection->recvHttpRequest();
                        switch ($request->getPath()) {
                            case '/':
                                $connection->respond();
                                break;
                            case '/greeter':
                                $connection->respond('Hello Swow');
                                break;
                            case '/echo':
                                $connection->respond($request->getBodyAsString());
                                break;
                            case '/echo_all':
                                $connection->respond($request->toString());
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
                                                    $connection->sendString(WebSocketFrame::PONG);
                                                    break;
                                                case WebSocketOpcode::PONG:
                                                    break;
                                                case WebSocketOpcode::CLOSE:
                                                    break 2;
                                                default:
                                                    $frame->getPayloadData()->rewind()->write("You said: {$frame->getPayloadData()}");
                                                    $connection->sendWebSocketFrame($frame);
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
                    if (!$request || !$request->getKeepAlive()) {
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
