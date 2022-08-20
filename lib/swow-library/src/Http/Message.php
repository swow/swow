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
use Swow\Object\DupTrait;

use function array_merge;
use function implode;
use function is_array;
use function strpos;
use function strtolower;
use function substr;
use function trim;

class Message implements MessageInterface, Stringable
{
    use DupTrait;

    public const DEFAULT_PROTOCOL_VERSION = '1.1';

    protected string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION;

    /**
     * headers holder format like `[ 'X-Header' => [ 'value 1', 'value 2' ] ]`
     *
     * @var array<string, array<string>>
     */
    protected array $headers = [];

    /**
     * headers names holder format like `[ 'x-header' => 'X-Header' ]`
     *
     * @var array<string, string>
     */
    protected array $headerNames = [];

    protected bool $keepAlive = true;

    protected ?Buffer $body = null;

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
            $this->setBody($body);
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
        return implode(', ', $this->getHeader($name));
    }

    /** @param string|array<string>|null $value */
    public function setHeader(string $name, mixed $value): static
    {
        $lowercaseName = strtolower($name);
        $rawName = $this->headerNames[$lowercaseName] ?? null;
        if ($rawName !== null) {
            unset($this->headers[$rawName]);
        }

        if ($value !== null) {
            $this->headers[$name] = is_array($value) ? $value : [(string) $value];
            $this->headerNames[$lowercaseName] = $name;
        } else {
            unset($this->headerNames[$lowercaseName]);
        }

        return $this;
    }

    /**
     * @param string $name
     * @param string|string[] $value
     */
    public function withHeader($name, $value): static
    {
        return (clone $this)->setHeader($name, $value);
    }

    /** @param string|array<string>|null $value */
    public function addHeader(string $name, mixed $value): static
    {
        $lowercaseName = strtolower($name);
        $rawName = $this->headerNames[$lowercaseName] ?? null;

        if ($rawName !== null) {
            if (is_array($value)) {
                $this->headers[$rawName] = array_merge($this->headers[$rawName], $value);
            } else {
                $this->headers[$rawName][] = $value;
            }
        } else {
            $this->headers[$rawName] = is_array($value) ? $value : [(string) $value];
        }

        return $this;
    }

    /**
     * @param string $name
     * @param string|string[] $value
     */
    public function withAddedHeader($name, $value): static
    {
        return (clone $this)->addHeader($name, $value);
    }

    public function unsetHeader(string $name): static
    {
        return $this->setHeader($name, null);
    }

    /**
     * @param string $name
     */
    public function withoutHeader($name): static
    {
        return (clone $this)->setHeader($name, null);
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
        return (clone $this)->setHeaders($headers);
    }

    public function getContentLength(): int
    {
        return $this->hasHeader('content-length') ?
            (int) $this->getHeaderLine('content-length') :
            $this->getBody()->getLength();
    }

    public function getContentType(): string
    {
        $contentTypeLine = $this->getHeaderLine('content-type');
        if (($pos = strpos($contentTypeLine, ';')) !== false) {
            // e.g. application/json; charset=UTF-8
            $contentType = strtolower(trim(substr($contentTypeLine, 0, $pos)));
        } else {
            $contentType = strtolower($contentTypeLine);
        }

        return $contentType;
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

    public function hasBody(): bool
    {
        return isset($this->body) && !$this->body->isEmpty();
    }

    public function getBody(): Buffer
    {
        return $this->body ??= new Buffer(0);
    }

    /** @Notice MUST clone the object before you change the body's content */
    public function setBody(StreamInterface $body): static
    {
        $this->body = Buffer::for($body);

        return $this;
    }

    public function withBody(StreamInterface $body): static
    {
        if ($body === $this->body) {
            return $this;
        }

        return (clone $this)->setBody($body);
    }

    public function toString(bool $headOnly = false): string
    {
        return packMessage(
            $this->getHeaders(),
            (!$headOnly && $this->hasBody()) ? (string) $this->getBody() : ''
        );
    }

    public function __toString(): string
    {
        return $this->toString();
    }
}
