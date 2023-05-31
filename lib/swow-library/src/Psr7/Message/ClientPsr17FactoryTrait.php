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

use Psr\Http\Message\RequestFactoryInterface;
use Psr\Http\Message\ResponseFactoryInterface;
use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\UriFactoryInterface;
use Swow\Psr7\Psr7;

trait ClientPsr17FactoryTrait
{
    use MessagePsr17FactoryTrait;

    protected RequestFactoryInterface $requestFactory;
    protected ResponseFactoryInterface $responseFactory;

    protected function __constructClientPsr17Factory(
        ?UriFactoryInterface $uriFactory = null,
        ?StreamFactoryInterface $streamFactory = null,
        ?RequestFactoryInterface $requestFactory = null,
        ?ResponseFactoryInterface $responseFactory = null
    ): void {
        $this->__constructMessagePsr17Factory($uriFactory, $streamFactory);
        $this->requestFactory = $requestFactory ?? Psr7::getDefaultRequestFactory();
        $this->responseFactory = $responseFactory ?? Psr7::getDefaultResponseFactory();
    }

    public function getRequestFactory(): RequestFactoryInterface
    {
        return $this->requestFactory;
    }

    public function setRequestFactory(RequestFactoryInterface $requestFactory): static
    {
        $this->requestFactory = $requestFactory;

        return $this;
    }

    public function getResponseFactory(): ResponseFactoryInterface
    {
        return $this->responseFactory;
    }

    public function setResponseFactory(ResponseFactoryInterface $responseFactory): static
    {
        $this->responseFactory = $responseFactory;

        return $this;
    }
}
