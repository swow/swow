<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

ini_set('memory_limit', '1G');

require __DIR__ . '/../../tools/autoload.php';

use Swow\Coroutine;
use Swow\Coroutine\Exception as CoroutineException;
use Swow\Http\Exception as HttpException;
use Swow\Http\Server as HttpServer;
use Swow\Http\Server\WebSocketFrame;
use Swow\Http\Status as HttpStatus;
use Swow\Socket\Exception as SocketException;
use Swow\WebSocket\Opcode as WebSocketOpcode;
use const Swow\Errno\EMFILE;
use const Swow\Errno\ENFILE;
use const Swow\Errno\ENOMEM;

$server = new HttpServer();
$server->bind('0.0.0.0', 9764)->listen();
while (true) {
    try {
        $connection = $server->acceptConnection();
        Coroutine::run(function () use ($connection) {
            try {
                while (true) {
                    $request = null;
                    try {
                        $request = $connection->recvHttpRequest();
                        switch ($request->getPath()) {
                            case '/':
                            {
                                $connection->respond();
                                break;
                            }
                            case '/greeter':
                            {
                                $connection->respond('Hello Swow');
                                break;
                            }
                            case '/echo':
                            {
                                $connection->respond($request->getBodyAsString());
                                break;
                            }
                            case '/chat':
                            {
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
                                    throw new HttpException(HttpStatus::BAD_REQUEST, 'Unsupported Upgrade Type');
                                }
                                $connection->respond(file_get_contents(__DIR__ . '/chat.html'));
                                break;
                            }
                            default:
                            {
                                $connection->error(HttpStatus::NOT_FOUND);
                            }
                        }
                    } catch (HttpException $exception) {
                        $connection->error($exception->getCode(), $exception->getMessage());
                    }
                    if (!$request || !$request->getKeepAlive()) {
                        break;
                    }
                }
            } catch (Exception $exception) {
                // you can log error here
            } finally {
                $connection->close();
            }
        });
    } catch (SocketException | CoroutineException $exception) {
        if (in_array($exception->getCode(), [EMFILE, ENFILE, ENOMEM], true)) {
            sleep(1);
        } else {
            break;
        }
    }
}
