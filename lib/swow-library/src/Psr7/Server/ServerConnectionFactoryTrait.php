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

use Swow\Psr7\Psr7;

trait ServerConnectionFactoryTrait
{
    protected ServerConnectionFactoryInterface $serverConnectionFactory;

    protected function __constructServerConnectionFactory(?ServerConnectionFactoryInterface $serverConnectionFactory = null): void
    {
        $this->serverConnectionFactory = $serverConnectionFactory ?? Psr7::getDefaultServerConnectionFactory();
    }

    public function getServerConnectionFactory(): ServerConnectionFactoryInterface
    {
        return $this->serverConnectionFactory;
    }

    public function setServerConnectionFactory(ServerConnectionFactoryInterface $serverConnectionFactory): static
    {
        $this->serverConnectionFactory = $serverConnectionFactory;

        return $this;
    }
}
