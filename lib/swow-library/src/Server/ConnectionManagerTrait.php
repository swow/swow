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

namespace Swow\Server;

use Swow\Socket;

trait ConnectionManagerTrait
{
    protected array $connections = [];

    protected function online(Socket $connection): void
    {
        $this->connections[$connection->getFd()] = $connection;
    }

    public function offline(int $fd): void
    {
        unset($this->connections[$fd]);
    }

    public function closeSessions(): void
    {
        foreach ($this->connections as $connection) {
            $connection->close();
        }
        $this->connections = [];
    }

    public function close(): bool
    {
        $ret = parent::close();
        $this->closeSessions();

        return $ret;
    }

    public function __destruct()
    {
        $this->closeSessions();
    }
}
