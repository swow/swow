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

namespace Swow\Tests\Psr7\Server;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Server\FilteredServerConnectionIterator;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Server\ServerConnection;
use Swow\Psr7\Server\ServerConnectionIterator;
use Swow\Socket;

/** @internal */
#[CoversClass(Server::class)]
#[CoversClass(ServerConnection::class)]
#[CoversClass(ServerConnectionIterator::class)]
#[CoversClass(FilteredServerConnectionIterator::class)]
final class ServerConnectionManagerTest extends TestCase
{
    public function testRegistrationFilteringAndCloseOwnership(): void
    {
        $server = new Server();
        $server->setMaxHeaderLength(4096)->setMaxContentLength(8192)->setRecvMessageTimeout(1000);
        $server->bind('127.0.0.1')->listen();
        $peers = [];
        $connections = [];
        try {
            foreach (range(1, 2) as $_) {
                $peer = new Socket(Socket::TYPE_TCP);
                $peers[] = $peer;
                $peer->setTimeout(1000)->connect($server->getSockAddress(), $server->getSockPort());
                $connections[] = $server->acceptConnection(1000);
            }
            [$first, $second] = $connections;
            $this->assertSame($server, $first->getServer());
            $this->assertSame(4096, $first->getMaxHeaderLength());
            $this->assertSame(8192, $first->getMaxContentLength());
            $this->assertSame(1000, $first->getRecvMessageTimeout());
            $this->assertSame([
                $first->getId() => $first,
                $second->getId() => $second,
            ], iterator_to_array($server->getConnections()));
            $this->assertCount(2, iterator_to_array($server->getHttpConnections()));
            $this->assertSame([], iterator_to_array($server->getWebSocketConnections()));
            $filtered = $server->getFilteredConnections(static fn(ServerConnection $connection): bool => $connection === $second);
            $this->assertSame([$second->getId() => $second], iterator_to_array($filtered));
            $this->assertSame([$second->getId() => $second], iterator_to_array($filtered));
            $this->assertSame([], iterator_to_array($server->getFilteredConnections(static fn(): bool => false)));
            $server->offline($first);
            $this->assertSame([$second->getId() => $second], iterator_to_array($server->getConnections()));
            $server->closeConnections();
            $this->assertSame([], iterator_to_array($server->getConnections()));
            $this->assertFalse($second->getSocket()->isAvailable());
            $this->assertTrue($first->getSocket()->isAvailable());
            $this->assertTrue($server->getSocket()->isAvailable());
        } finally {
            foreach ($connections as $connection) {
                if ($connection->getSocket()->isAvailable()) {
                    $connection->close();
                }
            }
            foreach ($peers as $peer) {
                $peer->close();
            }
            $server->close();
        }
    }
}
