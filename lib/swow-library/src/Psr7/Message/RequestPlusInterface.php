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

use Psr\Http\Message\UriInterface;

interface RequestPlusInterface extends MessagePlusInterface, \Psr\Http\Message\RequestInterface
{
    public function getMethod(): string;

    public function setMethod(string $method): static;

    public function withMethod(mixed $method): static;

    public function getUri(): UriInterface;

    public function setUri(UriInterface|string $uri, ?bool $preserveHost = null): static;

    public function withUri(UriInterface $uri, $preserveHost = null): static;

    public function getRequestTarget(): string;

    public function setRequestTarget(string $requestTarget): static;

    public function withRequestTarget(mixed $requestTarget): static;
}
