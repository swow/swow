<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Http;

use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\UriInterface;

class Request extends Message implements RequestInterface
{
    protected const PRESERVE_HOST = false;

    /**
     * @var string
     */
    protected $method = 'GET';

    /**
     * @var Uri
     */
    protected $uri;

    /**
     * @var string
     */
    protected $uriString = '';

    /**
     * @var null|string
     */
    protected $requestTarget;

    /**
     * @param string $method HTTP method
     * @param string|UriInterface $uri URI
     * @param array $headers Request headers
     * @param null|resource|StreamInterface|string $body Request body
     * @param string $protocolVersion Protocol version
     */
    public function __construct(string $method = '', $uri = '', array $headers = [], $body = '', string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION)
    {
        if ($method !== '') {
            $this->setMethod($method);
        }

        if (!($uri instanceof UriInterface)) {
            $uri = new Uri($uri);
        }
        $this->setUri($uri);

        parent::__construct($headers, $body, $protocolVersion);
    }

    public function getMethod(): string
    {
        return $this->method;
    }

    public function setMethod(string $method): self
    {
        $this->method = $method;

        return $this;
    }

    public function withMethod($method): self
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
        $uri = $this->uri;

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
            return $this->uri = new Uri($this->uriString);
        }

        return $this->uri;
    }

    public function setUri(UriInterface $uri, ?bool $preserveHost = null): Message
    {
        $this->uri = $uri;

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

    public function setUriString(string $uriString): self
    {
        $this->uriString = $uriString;

        return $this;
    }

    public function getUriAsString(): string
    {
        if ($this->uri !== null) {
            return $this->uri->toString();
        }

        return $this->uriString;
    }

    public function getStandardHeaders(): array
    {
        $headers = parent::getStandardHeaders();

        if (!$this->hasHeader('host')) {
            // Ensure Host is the first header.
            // See: http://tools.ietf.org/html/rfc7230#section-5.4
            $headers = ['Host' => $this->getHostFromUri()] + $headers;
        }

        return $headers;
    }

    public function getRequestTarget(): string
    {
        if ($this->requestTarget !== null) {
            return $this->requestTarget;
        }

        if (($target = $this->uri->getPath()) === '') {
            $target = '/';
        }
        if ($this->uri->getQuery() !== '') {
            $target .= '?' . $this->uri->getQuery();
        }

        return $target;
    }

    public function setRequestTarget(string $requestTarget): self
    {
        $this->requestTarget = $requestTarget;

        return $this;
    }

    public function withRequestTarget($requestTarget): self
    {
        $new = clone $this;
        $new->setRequestTarget($requestTarget);

        return $new;
    }

    public function toString(bool $headOnly = false): string
    {
        return packRequest(
            $this->getMethod(),
            $this->getUriAsString(),
            $this->getStandardHeaders(),
            !$headOnly ? $this->getBodyAsString() : '',
            $this->getProtocolVersion()
        );
    }
}
