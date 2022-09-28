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

use Psr\Http\Message\ResponseFactoryInterface;
use Psr\Http\Message\ServerRequestFactoryInterface;
use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\UploadedFileFactoryInterface;
use Psr\Http\Message\UriFactoryInterface;
use Swow\Psr7\Psr7;

trait ServerPsr17FactoryTrait
{
    use MessagePsr17FactoryTrait;

    protected ServerRequestFactoryInterface $serverRequestFactory;
    protected ResponseFactoryInterface $responseFactory;
    protected UploadedFileFactoryInterface $uploadedFileFactory;

    protected function __constructServerPsr17Factory(
        ?UriFactoryInterface $uriFactory = null,
        ?StreamFactoryInterface $streamFactory = null,
        ?ServerRequestFactoryInterface $serverRequestFactory = null,
        ?ResponseFactoryInterface $responseFactory = null,
        ?UploadedFileFactoryInterface $uploadedFileFactory = null
    ): void {
        $this->__constructMessagePsr17Factory($uriFactory, $streamFactory);
        $this->serverRequestFactory = $serverRequestFactory ?? Psr7::getDefaultPsr17Factory();
        $this->responseFactory = $responseFactory ?? Psr7::getDefaultPsr17Factory();
        $this->uploadedFileFactory = $uploadedFileFactory ?? Psr7::getDefaultPsr17Factory();
    }

    public function getServerRequestFactory(): ServerRequestFactoryInterface
    {
        return $this->serverRequestFactory;
    }

    public function setServerRequestFactory(ServerRequestFactoryInterface $serverRequestFactory): static
    {
        $this->serverRequestFactory = $serverRequestFactory;

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

    public function getUploadedFileFactory(): UploadedFileFactoryInterface
    {
        return $this->uploadedFileFactory;
    }

    public function setUploadedFileFactory(UploadedFileFactoryInterface $uploadedFileFactory): static
    {
        $this->uploadedFileFactory = $uploadedFileFactory;

        return $this;
    }
}
