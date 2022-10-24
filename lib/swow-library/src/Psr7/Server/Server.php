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
use Swow\Psr7\Config\LimitationTrait;
use Swow\Psr7\Message\ServerPsr17FactoryTrait;
use Swow\Psr7\Message\WebSocketFrameInterface;
use Swow\Socket;
use Swow\SocketException;

use function is_array;

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
        $connection = $this->serverConnectionFactory->createServerConnection($this);
        $this
            ->acceptTo($connection, $timeout)
            ->online($connection);
        $connection->addServerParams([
            'remote_addr' => $connection->getPeerAddress(),
            'remote_port' => $connection->getPeerPort(),
        ]);

        return $connection;
    }

    public function close(): bool
    {
        $ret = parent::close();
        $this->closeConnections();

        return $ret;
    }

    protected const BROADCAST_FLAG_NONE = 0;
    protected const BROADCAST_FLAG_RECORD_EXCEPTIONS = 1 << 0;

    /**
     * @param ServerConnection[] $targets
     * @param array<int, ServerConnection>|Closure(ServerConnection): bool $filter
     * @return SocketException[]|static
     * @psalm-todo: this is not correct: flags can be bitwise combined things
     * @psalm-return $flags is self::BROADCAST_FLAG_RECORD_EXCEPTIONS ? array<int, SocketException> : static
     */
    public function broadcastWebSocketFrame(WebSocketFrameInterface $frame, ?array $targets = null, array|Closure $filter = [], int $flags = self::BROADCAST_FLAG_NONE): static|array
    {
        $targets ??= $this->getWebSocketConnections();
        $exceptions = [];
        foreach ($targets as $target) {
            if (is_array($filter)) {
                if (isset($filter[$target->getId()])) {
                    continue;
                }
            } else { /* if ($filter instanceof Closure) */
                if (!$filter($target)) {
                    continue;
                }
            }
            try {
                $target->sendWebSocketFrame($frame);
            } catch (SocketException $exception) {
                if ($flags & static::BROADCAST_FLAG_RECORD_EXCEPTIONS) {
                    /* record it and ignore */
                    $exceptions[$target->getId()] = $exception;
                }
            }
        }

        return $flags & static::BROADCAST_FLAG_RECORD_EXCEPTIONS ? $exceptions : $this;
    }
}
