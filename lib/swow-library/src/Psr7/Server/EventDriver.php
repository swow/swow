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

namespace Swow\Psr7\Server;

use Closure;
use Exception;
use Psr\Http\Message\ResponseInterface;
use Swow;
use Swow\Coroutine;
use Swow\CoroutineException;
use Swow\Errno;
use Swow\Http\Protocol\ProtocolException as HttpProtocolException;
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Message\ServerRequestPlusInterface;
use Swow\Psr7\Message\UpgradeType;
use Swow\Psr7\Message\WebSocketFrameInterface;
use Swow\Psr7\Psr7;
use Swow\SocketException;
use Swow\WebSocket\Opcode as WebSocketOpcode;
use Swow\WebSocket\WebSocket;
use TypeError;

use function in_array;
use function is_array;
use function is_bool;
use function is_int;
use function sleep;
use function sprintf;
use function Swow\Debug\isStrictStringable;

class EventDriver
{
    protected Server $server;

    /** @var Closure(Server): void */
    protected Closure $startHandler;

    /** @var Closure(ServerConnection): void */
    protected Closure $connectionHandler;

    /** @var Closure(ServerConnection, ServerRequestPlusInterface): mixed */
    protected Closure $requestHandler;

    /** @var Closure(ServerConnection, ServerRequestPlusInterface, int): mixed */
    protected Closure $upgradeHandler;

    /** @var Closure(ServerConnection, WebSocketFrameInterface): mixed */
    protected Closure $messageHandler;

    /** @var Closure(ServerConnection): void */
    protected Closure $closeHandler;

    /** @var Closure(ServerConnection, Exception): void */
    protected Closure $exceptionHandler;

    public function __construct(?Server $server = null)
    {
        $this->server = $server ?? new Server();
    }

    /** @param callable(Server): void $handler */
    public function withStartHandler(callable $handler): static
    {
        $new = clone $this;
        $new->startHandler = Closure::fromCallable($handler);
        return $new;
    }

    /** @param callable(ServerConnection): void $handler */
    public function withConnectionHandler(callable $handler): static
    {
        $new = clone $this;
        $new->connectionHandler = Closure::fromCallable($handler);
        return $new;
    }

    /** @param callable(ServerConnection, ServerRequestPlusInterface): mixed $handler */
    public function withRequestHandler(callable $handler): static
    {
        $new = clone $this;
        $new->requestHandler = Closure::fromCallable($handler);
        return $new;
    }

    /** @param callable(ServerConnection, WebSocketFrameInterface): mixed $handler */
    public function withMessageHandler(callable $handler): static
    {
        $new = clone $this;
        $new->messageHandler = Closure::fromCallable($handler);
        return $new;
    }

    /** @param callable(ServerConnection): void $handler */
    public function withCloseHandler(callable $handler): static
    {
        $new = clone $this;
        $new->closeHandler = Closure::fromCallable($handler);
        return $new;
    }

    /** @param callable(ServerConnection, Exception): void $handler */
    public function withExceptionHandler(callable $handler): static
    {
        $new = clone $this;
        $this->exceptionHandler = $handler;
        return $new;
    }

    public function startOn(string $name, int $port): void
    {
        $server = $this->server->bind($name, $port)->listen();
        $connectionHandler = $this->connectionHandler ?? null;
        $requestHandler = $this->requestHandler ?? null;
        $upgradeHandler = $this->upgradeHandler ?? null;
        $messageHandler = $this->messageHandler ?? null;
        $closeHandler = $this->closeHandler ?? null;
        $exceptionHandler = $this->exceptionHandler ?? null;

        if (isset($this->startHandler)) {
            ($this->startHandler)($server);
        }

        while (true) {
            try {
                $connection = null;
                $connection = $server->acceptConnection();
                if ($connectionHandler !== null) {
                    $connectionHandler($connection);
                }
                Coroutine::run(static function () use ($connection, $requestHandler, $upgradeHandler, $messageHandler, $closeHandler, $exceptionHandler): void {
                    try {
                        while (true) {
                            $request = null;
                            try {
                                /** @var ServerRequestPlusInterface $request */
                                $request = $connection->recvHttpRequest();
                                if ($requestHandler) {
                                    $upgradeType = UpgradeType::UPGRADE_TYPE_NONE;
                                    if ($upgradeHandler !== null || $messageHandler !== null) {
                                        $upgradeType = Psr7::detectUpgradeType($request);
                                        if ($upgradeType !== UpgradeType::UPGRADE_TYPE_NONE) {
                                            if (($upgradeType & UpgradeType::UPGRADE_TYPE_WEBSOCKET) === 0) {
                                                throw new HttpProtocolException(HttpStatus::BAD_REQUEST, 'Unsupported Upgrade Type');
                                            }
                                            if ($upgradeHandler !== null) {
                                                $upgradeResponse = $upgradeHandler($connection, $request, $upgradeType);
                                                if ($upgradeResponse !== null && !($upgradeResponse instanceof ResponseInterface)) {
                                                    $upgradeResponse = static::solveUpgradeResponse($upgradeResponse);
                                                }
                                            }
                                        }
                                    }
                                    if ($upgradeType === UpgradeType::UPGRADE_TYPE_NONE) {
                                        $response = $requestHandler($connection, $request);
                                        if ($response !== null) {
                                            if ($response instanceof ResponseInterface) {
                                                $connection->sendHttpResponse($response);
                                            } elseif (is_array($response)) {
                                                $connection->respond(...$response);
                                            } else {
                                                $connection->respond($response);
                                            }
                                        }
                                    } elseif ($upgradeType & UpgradeType::UPGRADE_TYPE_WEBSOCKET) {
                                        $connection->upgradeToWebSocket($request, $upgradeResponse ?? null);
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
                                                    break 3;
                                                default:
                                                    $reply = $messageHandler($connection, $frame);
                                                    if ($reply instanceof WebSocketFrameInterface) {
                                                        $connection->sendWebSocketFrame($reply);
                                                    } elseif (Swow\Debug\isStrictStringable($reply)) {
                                                        $connection->sendWebSocketFrame(
                                                            Psr7::createWebSocketTextFrame(
                                                                payloadData: $reply
                                                            )
                                                        );
                                                    }
                                            }
                                        }
                                    }
                                }
                            } catch (HttpProtocolException $exception) {
                                $connection->error($exception->getCode(), $exception->getMessage(), close: true);
                                break;
                            }
                            if (!$connection->shouldKeepAlive()) {
                                break;
                            }
                        }
                    } catch (Exception $exception) {
                        if ($exceptionHandler !== null) {
                            $exceptionHandler($connection, $exception);
                        }
                    } finally {
                        if ($closeHandler !== null) {
                            $closeHandler($connection);
                        }
                        $connection->close();
                    }
                });
            } catch (CoroutineException|SocketException $exception) {
                if (in_array($exception->getCode(), [Errno::EMFILE, Errno::ENFILE, Errno::ENOMEM], true)) {
                    sleep(1);
                } else {
                    break;
                }
            }
        }
    }

    protected function solveUpgradeResponse(mixed $upgradeResponse): ResponseInterface
    {
        switch (true) {
            case is_bool($upgradeResponse):
                if (!$upgradeResponse) {
                    $upgradeResponse = Psr7::createResponse(HttpStatus::BAD_REQUEST);
                }
                break;
            case is_int($upgradeResponse):
                $upgradeResponse = Psr7::createResponse($upgradeResponse);
                break;
            case is_array($upgradeResponse):
                $statusCode = HttpStatus::OK;
                $headers = [];
                $body = '';
                $upgradeResponseArgs = $upgradeResponse;
                foreach ($upgradeResponseArgs as $upgradeResponseArg) {
                    if (isStrictStringable($upgradeResponseArg)) {
                        $body = (string) $upgradeResponseArg;
                    } elseif (is_int($upgradeResponseArg)) {
                        $statusCode = $upgradeResponseArg;
                    } elseif (is_array($upgradeResponseArg)) {
                        $headers = $upgradeResponseArg;
                    } else {
                        throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($upgradeResponseArg)));
                    }
                }
                $upgradeResponse = Psr7::createResponse(
                    code: $statusCode,
                    headers: $headers,
                    body: $body
                );
                break;
            case isStrictStringable($upgradeResponse):
                $upgradeResponse = Psr7::createResponse(
                    body: $upgradeResponse
                );
                break;
            default:
                break;
        }
        return $upgradeResponse;
    }
}
