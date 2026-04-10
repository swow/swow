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
use Psr\Http\Message\ServerRequestInterface;
use Swow;
use Swow\Http\Protocol\ProtocolException as HttpProtocolException;
use Swow\Http\Protocol\H2Connection;
use Swow\Psr7\Message\ServerRequestPlusInterface;
use Swow\Psr7\Message\UpgradeType;
use Swow\Psr7\Message\WebSocketFrameInterface;
use Swow\Psr7\Psr7;
use Swow\Http\Status as HttpStatus;
use Swow\Socket;
use Swow\WebSocket\Opcode as WebSocketOpcode;
use Swow\WebSocket\WebSocket;
use Throwable;

use function strlen;
use function str_starts_with;

class EventDriverConnectionHandler
{
    /**
     * @param ?Closure(ServerConnection|H2ServerConnection): void $connectionHandler
     * @param ?Closure(ServerConnection|H2ServerConnection, ServerRequestPlusInterface): mixed $requestHandler
     * @param ?Closure(ServerConnection, ServerRequestPlusInterface, int): mixed $upgradeHandler
     * @param ?Closure(ServerConnection, WebSocketFrameInterface): mixed $messageHandler
     * @param ?Closure(ServerConnection|H2ServerConnection): void $closeHandler
     * @param ?Closure(ServerConnection|H2ServerConnection, Throwable): void $exceptionHandler
     */
    public function __construct(
        private ?Closure $connectionHandler,
        private ?Closure $requestHandler,
        private ?Closure $upgradeHandler,
        private ?Closure $messageHandler,
        private ?Closure $closeHandler,
        private ?Closure $exceptionHandler,
    ) {
    }

    public function handle(ServerConnection|H2ServerConnection $connection): void
    {
        $runtimeConnection = $connection instanceof ServerConnection
            ? $this->detectDirectH2Connection($connection) ?? $connection
            : $connection;

        try {
            if ($this->connectionHandler !== null) {
                ($this->connectionHandler)($runtimeConnection);
            }

            if ($runtimeConnection instanceof H2ServerConnection) {
                $this->handleH2Connection($runtimeConnection);
                return;
            }

            $upgradedConnection = $this->handleHttpConnection($runtimeConnection);
            if ($upgradedConnection instanceof H2ServerConnection) {
                $runtimeConnection = $upgradedConnection;
            }
        } catch (Throwable $exception) {
            if ($this->exceptionHandler !== null) {
                ($this->exceptionHandler)($runtimeConnection, $exception);
            }
        } finally {
            if ($this->closeHandler !== null) {
                ($this->closeHandler)($runtimeConnection);
            }
            $runtimeConnection->close();
        }
    }

    private function handleH2Connection(H2ServerConnection $connection): void
    {
        if ($this->requestHandler === null) {
            return;
        }

        $connection->serve(function ($request, int $streamId) use ($connection) {
            return ($this->requestHandler)($connection, $request);
        });
    }

    protected function detectDirectH2Connection(ServerConnection $connection): ?H2ServerConnection
    {
        $negotiatedProtocol = $this->getNegotiatedAlpnProtocol($connection);
        if ($negotiatedProtocol === 'h2') {
            return $this->createH2ServerConnectionFromSocket($connection, $connection->getServerParams());
        }
        if ($negotiatedProtocol !== null) {
            return null;
        }

        $preface = $this->peekConnectionPreface($connection);
        if (!$this->isDirectH2PrefaceCandidate($preface)) {
            return null;
        }

        return $this->createH2ServerConnectionFromSocket($connection, $connection->getServerParams());
    }

    protected function getNegotiatedAlpnProtocol(ServerConnection $connection): ?string
    {
        if (!method_exists($connection, 'getNegotiatedAlpnProtocol')) {
            return null;
        }

        $protocol = $connection->getNegotiatedAlpnProtocol();
        return $protocol === '' ? null : $protocol;
    }

    protected function peekConnectionPreface(ServerConnection $connection): string
    {
        return $connection->peekString(strlen(H2Connection::CLIENT_PREFACE), $connection->getServer()->getRecvMessageTimeout());
    }

    protected function isDirectH2PrefaceCandidate(string $preface): bool
    {
        return $preface !== '' && str_starts_with(H2Connection::CLIENT_PREFACE, $preface);
    }

    protected function createH2ServerConnectionFromSocket(Socket $socket, array $serverParams = []): H2ServerConnection
    {
        return H2ServerConnection::wrap($socket, $serverParams);
    }

    private function handleHttpConnection(ServerConnection $connection): ?H2ServerConnection
    {
        while (true) {
            $request = null;
            try {
                /** @var ServerRequestPlusInterface $request */
                $request = $connection->recvHttpRequest();
                if ($this->requestHandler !== null) {
                    $upgradeType = Psr7::detectUpgradeType($request);
                    if (($upgradeType & UpgradeType::UPGRADE_TYPE_H2C) !== 0) {
                        return $this->handleH2cUpgrade($connection, $request);
                    }

                    $upgradeType = UpgradeType::UPGRADE_TYPE_NONE;
                    if ($this->upgradeHandler !== null || $this->messageHandler !== null) {
                        $upgradeType = Psr7::detectUpgradeType($request);
                        if ($upgradeType !== UpgradeType::UPGRADE_TYPE_NONE) {
                            if (($upgradeType & UpgradeType::UPGRADE_TYPE_WEBSOCKET) === 0) {
                                throw new HttpProtocolException(HttpStatus::BAD_REQUEST, 'Unsupported Upgrade Type');
                            }
                            if ($this->upgradeHandler !== null) {
                                $upgradeResponse = ($this->upgradeHandler)($connection, $request, $upgradeType);
                                if ($upgradeResponse !== null && !($upgradeResponse instanceof ResponseInterface)) {
                                    $upgradeResponse = H2ResponseNormalizer::normalize($upgradeResponse);
                                }
                            }
                        }
                    }
                    if ($upgradeType === UpgradeType::UPGRADE_TYPE_NONE) {
                        $response = ($this->requestHandler)($connection, $request);
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
                                    $reply = ($this->messageHandler)($connection, $frame);
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

        return null;
    }

    private function handleH2cUpgrade(ServerConnection $connection, ServerRequestInterface $request): H2ServerConnection
    {
        $settingsPayload = H2UpgradeBridge::decodeSettingsPayload($request);
        $connection->sendHttpHeader(
            HttpStatus::SWITCHING_PROTOCOLS,
            headers: [
                'Connection' => 'Upgrade',
                'Upgrade' => 'h2c',
            ]
        );

        $h2Connection = $this->createH2ServerConnectionFromSocket($connection, $connection->getServerParams());
        $h2Connection->getConnection()->initializeForUpgrade(peerSettingsPayload: $settingsPayload);
        H2UpgradeBridge::serveUpgradeRequest(
            $h2Connection,
            $request,
            fn(ServerRequestInterface $request, int $streamId): mixed => ($this->requestHandler)($h2Connection, $request),
        );

        return $h2Connection;
    }

}
