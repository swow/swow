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
use WeakMap;

trait ServerConnectionManagerTrait
{
    /** @var WeakMap<ServerConnection, bool> */
    protected WeakMap $connections;

    protected function __constructServerConnectionManager(): void
    {
        $this->connections = new WeakMap();
    }

    public function getConnections(): ServerConnectionIteratorInterface
    {
        return new ServerConnectionIterator($this->connections->getIterator());
    }

    /** @param Closure(ServerConnection): bool $filter */
    public function getFilteredConnections(Closure $filter)
    {
        return new FilteredServerConnectionIterator($this->connections->getIterator(), $filter);
    }

    public function getHttpConnections(): ServerConnectionIteratorInterface
    {
        return $this->getFilteredConnections(static function (ServerConnection $connection) {
            return $connection->getProtocolType() === $connection::PROTOCOL_TYPE_HTTP;
        });
    }

    public function getWebSocketConnections(): ServerConnectionIteratorInterface
    {
        return $this->getFilteredConnections(static function (ServerConnection $connection) {
            return $connection->getProtocolType() === $connection::PROTOCOL_TYPE_WEBSOCKET;
        });
    }

    protected function online(ServerConnection $connection): void
    {
        $this->connections[$connection] = $connection->getId();
    }

    public function offline(ServerConnection $connection): void
    {
        unset($this->connections[$connection]);
    }

    public function closeConnections(): void
    {
        $connections = $this->getConnections();
        $this->connections = new WeakMap();
        foreach ($connections as $connection) {
            $connection->close();
        }
    }
}
