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
use TypeError;
use function is_string;

class Request extends Message implements RequestInterface
{
    protected const PRESERVE_HOST = false;

    protected string $method = 'GET';

    protected ?UriInterface $uri = null;

    protected string $uriString = '';

    protected ?string $requestTarget = null;

    /**
     * @param string $method HTTP method
     * @param string|UriInterface $uri URI
     * @param array<string, array<string>|string> $headers Request headers
     * @param mixed $body Request body
     * @param string $protocolVersion Protocol version
     */
    public function __construct(string $method = '', $uri = '', array $headers = [], mixed $body = '', string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION)
    {
        if ($method !== '') {
            $this->setMethod($method);
        }

        if ($uri !== '') {
            if (is_string($uri)) {
                $this->setUriString($uri);
            } elseif ($uri instanceof UriInterface) {
                $this->setUri($uri);
            } else {
                throw new TypeError(sprintf(
                    'Argument #2 (\$uri) must be of type %s|string, array given',
                    UriInterface::class
                ));
            }
        }

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

    public function getUri(): UriInterface
    {
        if ($this->uri === null) {
            $this->setUri(new Uri($this->uriString));
        }

        return $this->uri;
    }

    public function setUri(UriInterface $uri, ?bool $preserveHost = null): Message
    {
        $this->uri = $uri;
        $this->uriString = '';

        if (!($preserveHost ?? static::PRESERVE_HOST) || !$this->hasHeader('host')) {
            $this->updateHostFromUri();
        }

        return $this;
    }

    public function withUri(UriInterface $uri, $preserveHost = null): Message
    {
        if ($uri === $this->uri) {
            return $this;
        }

        $new = clone $this;
        $new->setUri($uri, $preserveHost);

        return $new;
    }

    public function setUriString(string $uriString): static
    {
        $this->uriString = $uriString;
        $this->uri = null;

        return $this;
    }

    public function getUriAsString(): string
    {
        if ($this->uri !== null) {
            return (string) $this->uri;
        }

        return $this->uriString;
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

    protected function toStringEx(bool $headOnly = false, ?string $body = null): string
    {
        return packRequest(
            $this->getMethod(),
            $this->getUriAsString(),
            $this->getStandardHeaders(),
            !$headOnly ? ($body === null ? $this->getBodyAsString() : $body) : '',
            $this->getProtocolVersion()
        );
    }

    public function toString(bool $headOnly = false): string
    {
        return $this->toStringEx($headOnly);
    }
}
