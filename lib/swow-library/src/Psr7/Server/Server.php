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
use Swow\Psr7\Config\LimitationTrait;
use Swow\Psr7\Message\ServerPsr17FactoryTrait;
use Swow\Psr7\Message\WebSocketFrameInterface;
use Swow\Socket;
use Swow\SocketException;
use WeakMap;

class Server
{
    use LimitationTrait;

    use ServerConnectionFactoryTrait;

    use ServerConnectionManagerTrait;

    use ServerPsr17FactoryTrait;

    public const DEFAULT_BACKLOG = Socket::DEFAULT_BACKLOG;

    public const TYPE_TCP = Socket::TYPE_TCP;
    public const TYPE_TCP4 = Socket::TYPE_TCP4;
    public const TYPE_TCP6 = Socket::TYPE_TCP6;

    public const BIND_FLAG_NONE = Socket::BIND_FLAG_NONE;
    public const BIND_FLAG_IPV6ONLY = Socket::BIND_FLAG_IPV6ONLY;
    public const BIND_FLAG_REUSEADDR = Socket::BIND_FLAG_REUSEADDR;
    public const BIND_FLAG_REUSEPORT = Socket::BIND_FLAG_REUSEPORT;

    protected int $recvMessageTimeout = -1;

    private Socket $socket;

    private int $socketType;

    public function __construct(int $type = self::TYPE_TCP)
    {
        $this->socketType = $type;
        $this->socket = new Socket($type);
        $this->__constructServerConnectionManager();
        $this->__constructServerConnectionFactory();
        $this->__constructServerPsr17Factory();
    }

    public function getSocket(): Socket
    {
        return $this->socket;
    }

    public function getSocketType(): int
    {
        return $this->socketType;
    }

    public function close(): bool
    {
        return $this->socket->close();
    }

    public function __debugInfo(): array
    {
        return $this->socket->__debugInfo();
    }

    public function getRecvMessageTimeout(): int
    {
        return $this->recvMessageTimeout;
    }

    /**
     * @param int $timeout HTTP keep alive timeout in milliseconds
     */
    public function setRecvMessageTimeout(int $timeout): static
    {
        $this->recvMessageTimeout = $timeout;

        return $this;
    }

    public function getSockAddress(): string
    {
        return $this->getSocket()->getSockAddress();
    }

    public function getSockPort(): int
    {
        return $this->getSocket()->getSockPort();
    }

    public function getAcceptTimeout(): int
    {
        return $this->getSocket()->getAcceptTimeout();
    }

    public function setTimeout(int $timeout): static
    {
        $this->getSocket()->setTimeout($timeout);

        return $this;
    }

    public function setAcceptTimeout(int $timeout): static
    {
        $this->getSocket()->setAcceptTimeout($timeout);

        return $this;
    }

    public function setRecvBufferSize(int $size): static
    {
        $this->getSocket()->setRecvBufferSize($size);

        return $this;
    }

    public function setSendBufferSize(int $size): static
    {
        $this->getSocket()->setSendBufferSize($size);

        return $this;
    }

    public function setTcpNodelay(bool $enable): static
    {
        $this->getSocket()->setTcpNodelay($enable);

        return $this;
    }

    public function setTcpKeepAlive(bool $enable, int $delay): static
    {
        $this->getSocket()->setTcpKeepAlive($enable, $delay);

        return $this;
    }

    public function bind(string $name, int $port = 0, int $flags = self::BIND_FLAG_NONE): static
    {
        $this->getSocket()->bind($name, $port, $flags);

        return $this;
    }

    public function listen(int $backlog = self::DEFAULT_BACKLOG): static
    {
        $this->getSocket()->listen($backlog);

        return $this;
    }

    public function acceptConnection(?int $timeout = null): ServerConnection
    {
        $socket = $this->getSocket();
        while (true) {
            $connection = $this->serverConnectionFactory->createServerConnection($this);
            $socket->acceptTo($connection->getSocket(), $timeout);
            try {
                $connection->addServerParams([
                    'remote_addr' => $connection->getPeerAddress(),
                    'remote_port' => $connection->getPeerPort(),
                ]);
            } catch (SocketException) {
                /* FIXME: workaround for ENOTCONN error.
                 * getpeername() may return ENOTCONN in some edge cases,
                 * it may be caused by the client-side sent RST packet,
                 * we can not verify this behaviour because it is incidental,
                 * we ignore it and continue to accept next connection for now. */
                continue;
            }
            $this->online($connection);
            break;
        }

        return $connection;
    }

    public function rebind(string $name, int $port = 0, int $flags = self::BIND_FLAG_NONE): static
    {
        $this->socket->recreate($this->socketType);
        $this->__constructServerConnectionManager();

        return $this->bind($name, $port, $flags);
    }

    protected const BROADCAST_FLAG_NONE = 0;
    protected const BROADCAST_FLAG_RECORD_EXCEPTIONS = 1 << 0;

    /**
     * @param iterable<ServerConnection> $targets
     * @param ?Closure(ServerConnection): bool $filter
     */
    public function broadcastWebSocketFrame(WebSocketFrameInterface $frame, ?iterable $targets = null, ?Closure $filter = null, int $flags = self::BROADCAST_FLAG_NONE): BroadcastResult
    {
        $targets ??= $this->getConnections();
        $count = $failureCount = 0;
        $exceptions = null;
        foreach ($targets as $target) {
            if ($target->getProtocolType() !== $target::PROTOCOL_TYPE_WEBSOCKET) {
                continue;
            }
            if ($filter && !$filter($target)) {
                continue;
            }
            $count++;
            try {
                $target->sendWebSocketFrame($frame);
            } catch (Exception $exception) {
                if ($flags & static::BROADCAST_FLAG_RECORD_EXCEPTIONS) {
                    /* record it and ignore */
                    /** @var ?WeakMap<ServerConnection, Exception> $exceptions */
                    $exceptions ??= new WeakMap();
                    $exceptions[$target] = $exception;
                }
                $failureCount++;
            }
        }

        return new BroadcastResult($count, $failureCount, $exceptions);
    }
}
