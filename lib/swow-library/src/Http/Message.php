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

use Psr\Http\Message\MessageInterface;
use Psr\Http\Message\StreamInterface;
use Stringable;
use function implode;
use function is_array;
use function strtolower;

class Message implements MessageInterface, Stringable
{
    public const DEFAULT_PROTOCOL_VERSION = '1.1';

    protected string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION;

    /** @var array<string, array<string>> */
    protected array $headers = [];

    /** @var array<string>|null */
    protected ?array $headerNames = null;

    protected bool $keepAlive = true;

    protected ?\Swow\Http\Buffer $body = null;

    /**
     * @param array<string, array<string>|string> $headers Request headers
     * @param mixed $body Request body
     */
    public function __construct(array $headers = [], mixed $body = null, string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION)
    {
        $this->setProtocolVersion($protocolVersion);

        if (!empty($headers)) {
            $this->setHeaders($headers);
        }

        // If we got no body, defer initialization of the stream until getBody()
        if ($body !== null && $body !== '') {
            $this->setBody(Buffer::for($body));
        }
    }

    public function getProtocolVersion(): string
    {
        return $this->protocolVersion;
    }

    public function setProtocolVersion(string $protocolVersion): static
    {
        $this->protocolVersion = $protocolVersion;

        return $this;
    }

    public function withProtocolVersion($version): static
    {
        if ($version === $this->protocolVersion) {
            return $this;
        }

        $new = clone $this;
        $new->protocolVersion = $version;

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

    /** @param string|array<string>|null $value */
    public function setHeader(string $name, mixed $value): static
    {
        $lowerCaseName = strtolower($name);
        $rawName = $this->headerNames[$lowerCaseName] ?? null;
        if ($rawName !== null) {
            unset($this->headers[$rawName]);
        }

        if ($value !== null) {
            $this->headers[$name] = is_array($value) ? $value : [(string) $value];
            $this->headerNames[$lowerCaseName] = $name;
        } else {
            unset($this->headerNames[$lowerCaseName]);
        }

        return $this;
    }

    /** @return array<string, array<string>> $headers */
    public function getHeaders(): array
    {
        return $this->headers;
    }

    /** @return array<string, array<string>> $headers */
    public function getStandardHeaders(): array
    {
        $headers = $this->getHeaders();
        if (!$this->hasHeader('connection')) {
            $headers['Connection'] = [$this->getKeepAlive() ? 'keep-alive' : 'close'];
        }
        if (!$this->hasHeader('content-length')) {
            $headers['Content-Length'] = [(string) $this->getContentLength()];
        }

        return $headers;
    }

    /** @param array<string, array<string>|string> $headers */
    public function setHeaders(array $headers): static
    {
        foreach ($headers as $name => $value) {
            $this->setHeader($name, $value);
        }

        return $this;
    }

    /** @param array<string, array<string>|string> $headers */
    public function withHeaders(array $headers): static
    {
        $new = clone $this;
        $new->setHeaders($headers);

        return $new;
    }

    /**
     * @param string $name
     * @param string|string[] $value
     */
    public function withHeader($name, $value): static
    {
        $new = clone $this;
        $new->setHeader($name, $value);

        return $new;
    }

    /**
     * @param string $name
     * @param string|string[] $value
     */
    public function withAddedHeader($name, $value): static
    {
        $new = clone $this;
        $header = $new->getHeaderLine($name);
        if ($header !== '') {
            $value = $header . ', ' . $value;
        }
        $new->setHeader($name, $value);

        return $new;
    }

    /**
     * @param string $name
     */
    public function withoutHeader($name): static
    {
        $new = clone $this;
        $new->setHeader($name, null);

        return $new;
    }

    public function getKeepAlive(): bool
    {
        return $this->keepAlive;
    }

    public function setKeepAlive(bool $keepAlive): static
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

    /** @Notice MUST clone the object before you change the body's content */
    public function setBody(?Buffer $body): static
    {
        $this->body = $body;

        return $this;
    }

    public function withBody(StreamInterface $body): static
    {
        if ($body === $this->body) {
            return $this;
        }

        $new = clone $this;
        $new->setBody(Buffer::for($body));

        return $new;
    }

    public function toString(bool $headOnly = false): string
    {
        return packMessage(
            $this->getHeaders(),
            !$headOnly ? $this->getBodyAsString() : ''
        );
    }

    public function __toString(): string
    {
        return $this->toString();
    }
}
