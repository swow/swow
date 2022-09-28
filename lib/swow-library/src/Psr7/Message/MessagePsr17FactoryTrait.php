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

namespace Swow\Psr7\Message;

use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\UriFactoryInterface;
use Swow\Psr7\Psr7;

trait MessagePsr17FactoryTrait
{
    protected UriFactoryInterface $uriFactory;
    protected StreamFactoryInterface $streamFactory;

    protected function __constructMessagePsr17Factory(?UriFactoryInterface $uriFactory = null, ?StreamFactoryInterface $streamFactory = null): void
    {
        $this->uriFactory = $uriFactory ?? Psr7::getDefaultPsr17Factory();
        $this->streamFactory = $streamFactory ?? Psr7::getDefaultPsr17Factory();
    }

    public function getUriFactory(): UriFactoryInterface
    {
        return $this->uriFactory;
    }

    public function setUriFactory(UriFactoryInterface $uriFactory): static
    {
        $this->uriFactory = $uriFactory;

        return $this;
    }

    public function getStreamFactory(): StreamFactoryInterface
    {
        return $this->streamFactory;
    }

    public function setStreamFactory(StreamFactoryInterface $streamFactory): static
    {
        $this->streamFactory = $streamFactory;

        return $this;
    }
}
