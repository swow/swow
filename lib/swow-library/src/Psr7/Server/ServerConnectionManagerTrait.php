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

use WeakMap;

trait ServerConnectionManagerTrait
{
    /** @var WeakMap<ServerConnection, bool> */
    protected WeakMap $connections;

    protected function __constructServerConnectionManager(): void
    {
        $this->connections = new WeakMap();
    }

    public function getConnections(): ServerConnections
    {
        return new ServerConnections($this->connections->getIterator());
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
