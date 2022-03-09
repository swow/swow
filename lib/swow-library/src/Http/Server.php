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

namespace Swow\Http;

use Closure;
use Swow\Buffer;
use Swow\Http\Server\Connection;
use Swow\Server\ConnectionManagerTrait;
use Swow\Socket;
use Swow\SocketException;
use function is_array;
use function is_callable;
use function is_string;

class Server extends Socket
{
    use ConfigTrait;

    use ConnectionManagerTrait;

    public function __construct()
    {
        parent::__construct(static::TYPE_TCP);
    }

    public function acceptConnection(?int $timeout = null): Connection
    {
        $connection = new Connection($this);
        $this
            ->acceptTo($connection, $timeout)
            ->online($connection);

        return $connection;
    }

    protected const BROADCAST_FLAG_NONE = 0;
    protected const BROADCAST_FLAG_RECORD_EXCEPTIONS = 1 << 0;

    /**
     * @param Connection[] $targets
     * @param array<int, Connection>|callable $filter
     * @return SocketException[]|static
     * @psalm-todo: this is not correct: flags can be bitwise combined things
     * @psalm-return $flags is self::BROADCAST_FLAG_RECORD_EXCEPTIONS ? array<SocketException> : static
     */
    public function broadcastMessage(WebSocketFrame $frame, ?array $targets = null, array|callable $filter = [], int $flags = self::BROADCAST_FLAG_NONE): static|array
    {
        if ($targets === null) {
            /** @var Connection[] $targets */
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
            } else /* if (is_callable($filter)) */ {
                if (!$filter($target)) {
                    continue;
                }
            }
            try {
                if (is_string($frame)) {
                    $target->sendString($frame);
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
