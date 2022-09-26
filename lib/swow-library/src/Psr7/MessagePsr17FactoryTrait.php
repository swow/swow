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

namespace Swow\Psr7;

use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\UriFactoryInterface;

trait MessagePsr17FactoryTrait
{
    protected UriFactoryInterface $uriFactory;
    protected StreamFactoryInterface $streamFactory;

    protected function __constructMessagePsr17Factory(?UriFactoryInterface $uriFactory = null, ?StreamFactoryInterface $streamFactory = null): void
    {
        $this->uriFactory = $uriFactory ?? Psr17Factory::getInstance();
        $this->streamFactory = $streamFactory ?? Psr17Factory::getInstance();
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
