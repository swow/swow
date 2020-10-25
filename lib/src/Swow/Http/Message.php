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

use Psr\Http\Message\MessageInterface;
use Psr\Http\Message\StreamInterface;

class Message implements MessageInterface
{
    public const DEFAULT_PROTOCOL_VERSION = '1.1';

    /**
     * @var string
     */
    protected $protocolVersion = self::DEFAULT_PROTOCOL_VERSION;

    /**
     * @var string[][]
     */
    protected $headers = [];

    /**
     * @var null|array
     */
    protected $headerNames;

    /**
     * @var bool
     */
    protected $keepAlive = true;

    /**
     * @var Buffer
     */
    protected $body;

    /**
     * @param array $headers Request headers
     * @param null|Buffer|string $body Request body
     */
    public function __construct(array $headers = [], $body = '', string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION)
    {
        $this->setProtocolVersion($protocolVersion);

        if (!empty($headers)) {
            $this->setHeaders($headers);
        }

        // If we got no body, defer initialization of the stream until getBody()
        if ($body !== null && $body !== '') {
            $this->setBody(Buffer::create($body));
        }
    }

    public function getProtocolVersion(): string
    {
        return $this->protocolVersion;
    }

    public function setProtocolVersion(string $protocolVersion): self
    {
        $this->protocolVersion = $protocolVersion;

        return $this;
    }

    public function withProtocolVersion($protocolVersion): self
    {
        if ($protocolVersion === $this->protocolVersion) {
            return $this;
        }

        $new = clone $this;
        $new->protocolVersion = $protocolVersion;

        return $new;
    }

    public function hasHeader($name): bool
    {
        return isset($this->headerNames[strtolower($name)]);
    }

    public function getHeader($name): array
    {
        $name = $this->headerNames[strtolower($name)] ?? null;

        return $name !== null ? $this->headers[$name] : [];
    }

    public function getHeaderLine($name): string
    {
        return implode(',', $this->getHeader($name));
    }

    public function setHeader(string $name, $value): self
    {
        $lowerCaseName = strtolower($name);
        $rawName = $this->headerNames[$lowerCaseName] ?? null;
        if ($rawName !== null) {
            unset($this->headers[$rawName]);
        }

        if ($value !== null) {
            $this->headers[$name] = is_array($value) ? $value : [$value];
            $this->headerNames[$lowerCaseName] = $name;
        } else {
            unset($this->headerNames[$lowerCaseName]);
        }

        return $this;
    }

    public function getHeaders(): array
    {
        return $this->headers;
    }

    public function getStandardHeaders(): array
    {
        $headers = $this->getHeaders();
        if (!$this->hasHeader('connection')) {
            $headers['Connection'] = [$this->getKeepAlive() ? 'Keep-Alive' : 'Closed'];
        }
        if (!$this->hasHeader('content-length')) {
            $headers['Content-Length'] = [(string) $this->getContentLength()];
        }

        return $headers;
    }

    public function setHeaders(array $headers): self
    {
        foreach ($headers as $name => $value) {
            $this->setHeader($name, $value);
        }

        return $this;
    }

    public function withHeaders(array $headers): self
    {
        $new = clone $this;
        $new->setHeaders($headers);

        return $new;
    }

    public function withHeader($name, $value): self
    {
        $new = clone $this;
        $new->setHeader($name, $value);

        return $new;
    }

    public function withAddedHeader($name, $value): self
    {
        $new = clone $this;
        $header = $new->getHeaderLine($name);
        if ($header !== '') {
            $value = $header . ', ' . $value;
        }
        $new->setHeader($name, $value);

        return $new;
    }

    public function withoutHeader($name): self
    {
        $new = clone $this;
        $new->setHeader($name, null);

        return $new;
    }

    public function getKeepAlive(): bool
    {
        return $this->keepAlive;
    }

    public function setKeepAlive(bool $keepAlive): self
    {
        $this->keepAlive = $keepAlive;

        return $this;
    }

    public function getContentLength(): int
    {
        if ($this->body === null) {
            return 0;
        }

        return $this->body->getLength();
    }

    public function hasBody(): bool
    {
        return $this->body !== null;
    }

    public function getBody(): Buffer
    {
        return $this->body ?? ($this->body = new Buffer(0));
    }

    public function getBodyAsString(): string
    {
        return $this->body ? $this->body->toString() : '';
    }

    /* @Notice MUST clone the object before you change the body's content */
    public function setBody(?Buffer $body)
    {
        $this->body = $body;

        return $this;
    }

    /**
     * @param Buffer|StreamInterface $body
     * @return Message
     */
    public function withBody(StreamInterface $body): self
    {
        if ($body === $this->body) {
            return $this;
        }

        $new = clone $this;
        $new->setBody($body);

        return $new;
    }

    public function toString(bool $headOnly = false): string
    {
        return packMessage(
            $this->getHeaders(),
            !$headOnly ? $this->getBodyAsString() : ''
        );
    }

    public function __toString()
    {
        return $this->toString();
    }
}
