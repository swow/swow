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

use Psr\Http\Message\UriInterface;
use Swow\Http;

use function strcasecmp;
use function strlen;

class Request extends Message implements RequestPlusInterface
{
    public const UPGRADE_NONE = 0;
    public const UPGRADE_WEBSOCKET = 1 << 0;
    public const UPGRADE_H2C = 1 << 1;
    public const UPGRADE_UNKNOWN = 1 << 31;

    protected const PRESERVE_HOST = false;

    protected string $method = 'GET';

    protected UriInterface $uri;

    protected ?string $requestTarget = null;

    public function getMethod(): string
    {
        return $this->method;
    }

    public function setMethod(string $method): static
    {
        $this->method = $method;

        return $this;
    }

    /**
     * @param string $method
     */
    public function withMethod(mixed $method): static
    {
        if ($method === $this->method) {
            return $this;
        }

        return (clone $this)->setMethod($method);
    }

    protected function getHostFromUri(): string
    {
        $uri = $this->getUri();

        $host = $uri->getHost();
        if ($host !== '') {
            $port = $uri->getPort();
            if ($port !== null) {
                $host .= ':' . $port;
            }
        }

        return $host;
    }

    protected function updateHostFromUri(): void
    {
        $host = $this->getHostFromUri();
        if ($host === '') {
            return;
        }
        // Ensure Host is the first header.
        // See: http://tools.ietf.org/html/rfc7230#section-5.4
        $this->headers = ['Host' => [$host]] + $this->headers;
        $this->headerNames['host'] = 'Host';
    }

    public function getUri(): UriInterface
    {
        return $this->uri ??= new Uri();
    }

    public function setUri(string|UriInterface $uri, ?bool $preserveHost = null): static
    {
        if (!($uri instanceof UriInterface)) {
            $uri = new Uri($uri);
        }
        $this->uri = $uri;

        if (!($preserveHost ?? static::PRESERVE_HOST) || !$this->hasHeader('host')) {
            $this->updateHostFromUri();
        }

        return $this;
    }

    public function withUri(string|UriInterface $uri, $preserveHost = null): static
    {
        if ($uri === $this->uri) {
            return $this;
        }

        return (clone $this)->setUri($uri, $preserveHost);
    }

    /** @return array<string, array<string>> */
    public function getStandardHeaders(): array
    {
        $headers = parent::getStandardHeaders();

        if (!$this->hasHeader('host')) {
            // Ensure Host is the first header.
            // See: http://tools.ietf.org/html/rfc7230#section-5.4
            $headers = ['Host' => [$this->getHostFromUri()]] + $headers;
        }

        return $headers;
    }

    public function getRequestTarget(): string
    {
        if ($this->requestTarget !== null) {
            return $this->requestTarget;
        }

        $uri = $this->getUri();

        if (($target = $uri->getPath()) === '') {
            $target = '/';
        }
        if ($uri->getQuery() !== '') {
            $target .= '?' . $uri->getQuery();
        }

        return $target;
    }

    public function setRequestTarget(string $requestTarget): static
    {
        $this->requestTarget = $requestTarget;

        return $this;
    }

    /**
     * @param string $requestTarget
     */
    public function withRequestTarget(mixed $requestTarget): static
    {
        return (clone $this)->setRequestTarget($requestTarget);
    }

    public function getUpgrade(): int
    {
        $upgrade = $this->getHeaderLine('upgrade');
        if ($upgrade === '') {
            $upgrade = static::UPGRADE_NONE;
        }
        if (strlen($upgrade) === strlen('websocket') && strcasecmp($upgrade, 'websocket') === 0) {
            $upgrade = static::UPGRADE_WEBSOCKET;
        } elseif (strlen($upgrade) === strlen('h2c') && strcasecmp($upgrade, 'h2c') === 0) {
            $upgrade = static::UPGRADE_H2C;
        } else {
            $upgrade = static::UPGRADE_UNKNOWN;
        }

        return $upgrade;
    }

    public function toString(bool $withoutBody = false): string
    {
        return Http::packRequest(
            $this->getMethod(),
            (string) $this->getUri(),
            $this->getStandardHeaders(),
            (!$withoutBody && $this->hasBody()) ? (string) $this->getBody() : '',
            $this->getProtocolVersion()
        );
    }
}
