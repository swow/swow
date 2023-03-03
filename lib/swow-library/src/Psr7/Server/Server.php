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

class Server extends Socket
{
    use LimitationTrait;

    use ServerConnectionFactoryTrait;

    use ServerConnectionManagerTrait;

    use ServerPsr17FactoryTrait;

    public function __construct(int $type = self::TYPE_TCP)
    {
        parent::__construct($type);
        $this->__constructServerConnectionManager();
        $this->__constructServerConnectionFactory();
        $this->__constructServerPsr17Factory();
    }

    public function acceptConnection(?int $timeout = null): ServerConnection
    {
        while (true) {
            $connection = $this->serverConnectionFactory->createServerConnection($this);
            $this->acceptTo($connection, $timeout);
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
