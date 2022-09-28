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
    /** @var WeakMap<ServerConnection> */
    protected WeakMap $connections;

    protected function __constructServerConnectionManager(): void
    {
        $this->connections = new WeakMap();
    }

    public function getConnections(): WeakMap
    {
        return $this->connections;
    }

    protected function online(ServerConnection $connection): void
    {
        $this->connections[$connection] = $connection;
    }

    public function offline(ServerConnection $connection): void
    {
        unset($this->connections[$connection]);
    }

    public function closeConnections(): void
    {
        foreach ($this->connections as $connection) {
            $connection->close();
        }
        $this->connections = new WeakMap();
    }
}
