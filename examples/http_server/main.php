<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/s-wow/swow
 * @contact  twosee <twose@qq.com>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

ini_set('memory_limit', '1G');

require __DIR__ . '/../../vendor/autoload.php';

use Swow\Coroutine;
use Swow\Coroutine\Exception as CoroutineException;
use Swow\Http\Exception as HttpException;
use Swow\Http\Server as HttpServer;
use Swow\Http\Status as HttpStatus;
use Swow\Socket\Exception as SocketException;
use Swow\WebSocket\Opcode as WebSocketOpcode;
use const Swow\EMFILE;
use const Swow\ENFILE;

$server = new HttpServer();
$server->bind('0.0.0.0', 9764)->listen();
while (true) {
    try {
        $session = $server->acceptSession();
        Coroutine::run(function () use ($session) {
            try {
                while (true) {
                    $request = null;
                    try {
                        $request = $session->recvHttpRequest();
                        switch ($request->getPath()) {
                            case '/':
                            {
                                $session->respond();
                                break;
                            }
                            case '/greeter':
                            {
                                $session->respond('Hello Swow');
                                break;
                            }
                            case '/echo':
                            {
                                $session->respond($request->getBodyAsString());
                                break;
                            }
                            case '/chat':
                            {
                                if ($upgrade = $request->getUpgrade()) {
                                    if ($upgrade === $request::UPGRADE_WEBSOCKET) {
                                        $session->upgradeToWebSocket($request);
                                        $request = null;
                                        while (true) {
                                            $frame = $session->recvWebSocketFrame();
                                            $opcode = $frame->getOpcode();
                                            switch ($opcode) {
                                                case WebSocketOpcode::PONG:
                                                    break;
                                                case WebSocketOpcode::CLOSE:
                                                    break 2;
                                                default:
                                                    $frame->getPayloadData()->rewind()->write("You said: {$frame->getPayloadData()}");
                                                    $session->sendWebSocketFrame($frame);
                                            }
                                        }
                                        break;
                                    }
                                    throw new HttpException(HttpStatus::BAD_REQUEST, 'Unsupported Upgrade Type');
                                }
                                $session->respond(file_get_contents(__DIR__ . '/chat.html'));
                                break;
                            }
                            default:
                            {
                                $session->error(HttpStatus::NOT_FOUND);
                            }
                        }
                    } catch (HttpException $exception) {
                        $session->error($exception->getCode(), $exception->getMessage());
                    }
                    if (!$request || !$request->getKeepAlive()) {
                        break;
                    }
                }
            } catch (Exception $exception) {
                // you can log error here
            } finally {
                $session->close();
            }
        });
    } catch (SocketException $exception) {
        break;
    } catch (CoroutineException $exception) {
        if (in_array($exception->getCode(), [EMFILE, ENFILE], true)) {
            sleep(1);
        } else {
            break;
        }
    }
}
