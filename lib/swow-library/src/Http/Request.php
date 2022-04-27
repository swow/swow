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

namespace Swow\Http;

use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\UriInterface;
use function strcasecmp;
use function strlen;

class Request extends Message implements RequestInterface
{
    public const UPGRADE_NONE = 0;
    public const UPGRADE_WEBSOCKET = 1 << 0;
    public const UPGRADE_H2C = 1 << 1;
    public const UPGRADE_UNKNOWN = 1 << 31;

    protected const PRESERVE_HOST = false;

    protected string $method = 'GET';

    protected UriInterface $uri;

    protected ?string $requestTarget = null;

    /** @param array<string, array<string>|string> $headers */
    public function __construct(string $method = '', string|UriInterface $uri = '', array $headers = [], mixed $body = '', string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION)
    {
        if ($method !== '') {
            $this->setMethod($method);
        }
        $this->setUri($uri);

        parent::__construct($headers, $body, $protocolVersion);
    }

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
    public function withMethod($method): static
    {
        if ($method === $this->method) {
            return $this;
        }

        $new = clone $this;
        $new->setMethod($method);

        return $new;
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

    public function getUri(): Uri
    {
        return $this->uri ??= new Uri();
    }

    public function setUri(string|UriInterface $uri, ?bool $preserveHost = null): static
    {
        $this->uri = Uri::from($uri);

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

        $new = clone $this;
        $new->setUri($uri, $preserveHost);

        return $new;
    }

    /**
     * @return array<string, array<string>>
     */
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
    public function withRequestTarget($requestTarget): static
    {
        $new = clone $this;
        $new->setRequestTarget($requestTarget);

        return $new;
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

    public function toString(bool $headOnly = false): string
    {
        return packRequest(
            $this->getMethod(),
            (string) $this->getUri(),
            $this->getStandardHeaders(),
            (!$headOnly && $this->hasBody()) ? (string) $this->getBody() : '',
            $this->getProtocolVersion()
        );
    }
}
