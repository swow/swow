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
use Swow\Buffer;
use Swow\Http\Config\LimitationTrait;
use Swow\Psr7\Message\ServerPsr17FactoryTrait;
use Swow\Psr7\Message\WebSocketFrame;
use Swow\Server\ConnectionManagerTrait;
use Swow\Socket;
use Swow\SocketException;

use function is_array;
use function is_callable;
use function is_string;

class Server extends Socket
{
    use ConnectionManagerTrait;

    use LimitationTrait;

    use ServerConnectionFactoryTrait;

    use ServerPsr17FactoryTrait;

    public function __construct(int $type = self::TYPE_TCP)
    {
        parent::__construct($type);
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

    protected const BROADCAST_FLAG_NONE = 0;
    protected const BROADCAST_FLAG_RECORD_EXCEPTIONS = 1 << 0;

    /**
     * @param ServerConnection[] $targets
     * @param array<int, ServerConnection>|callable $filter
     * @return SocketException[]|static
     * @psalm-todo: this is not correct: flags can be bitwise combined things
     * @psalm-return $flags is self::BROADCAST_FLAG_RECORD_EXCEPTIONS ? array<SocketException> : static
     */
    public function broadcastMessage(WebSocketFrame $frame, ?array $targets = null, array|callable $filter = [], int $flags = self::BROADCAST_FLAG_NONE): static|array
    {
        if ($targets === null) {
            /** @var ServerConnection[] $targets */
            $targets = $this->connections;
        }
        if ($frame->getPayloadLength() <= Buffer::PAGE_SIZE) {
            $frame = $frame->toString();
        }
        if (is_callable($filter)) {
            $filter = Closure::fromCallable($filter);
        }
        $exceptions = [];
        foreach ($targets as $target) {
            if ($target->getProtocolType() !== $target::PROTOCOL_TYPE_WEBSOCKET) {
                continue;
            }
            if (is_array($filter)) {
                if (isset($filter[$target->getId()])) {
                    continue;
                }
            } else { /* if (is_callable($filter)) */
                if (!$filter($target)) {
                    continue;
                }
            }
            try {
                if (is_string($frame)) {
                    $target->send($frame);
                } else {
                    $target->sendWebSocketFrame($frame);
                }
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
