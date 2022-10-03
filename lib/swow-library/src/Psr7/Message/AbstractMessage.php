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

use Error;
use Psr\Http\Message\StreamInterface;
use Swow\Object\StringableTrait;
use Swow\Psr7\Psr7;

use function array_merge;
use function implode;
use function is_array;
use function strpos;
use function strtolower;
use function substr;
use function trim;

abstract class AbstractMessage implements MessagePlusInterface
{
    use StringableTrait;

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

    protected bool $shouldKeepAlive = true;

    protected ?StreamInterface $body = null;

    public function getProtocolVersion(): string
    {
        return $this->protocolVersion;
    }

    public function setProtocolVersion(string $protocolVersion): static
    {
        $this->protocolVersion = $protocolVersion;

        return $this;
    }

    public function withProtocolVersion(mixed $version): static
    {
        if ($version === $this->protocolVersion) {
            return $this;
        }

        $new = clone $this;
        $new->protocolVersion = $version;

        return $new;
    }

    public function hasHeader(mixed $name): bool
    {
        return isset($this->headerNames[strtolower($name)]);
    }

    public function getHeader(mixed $name): array
    {
        $name = $this->headerNames[strtolower($name)] ?? null;

        return $name !== null ? $this->headers[$name] : [];
    }

    public function getHeaderLine(mixed $name): string
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
    public function withHeader(mixed $name, mixed $value): static
    {
        return (clone $this)->setHeader((string) $name, $value);
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
    public function withAddedHeader(mixed $name, mixed $value): static
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
            $headers['Connection'] = [$this->shouldKeepAlive() ? 'keep-alive' : 'close'];
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

    /**
     * @param array<string, array<string>|string> $headers
     * @param array<string, string> $headerNames
     */
    public function setHeadersAndHeaderNames(array $headers, array $headerNames): static
    {
        $this->headers = $headers;
        $this->headerNames = $headerNames;

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
            $this->getBody()->getSize();
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

    public function shouldKeepAlive(): bool
    {
        return $this->shouldKeepAlive;
    }

    public function setShouldKeepAlive(bool $shouldKeepAlive): static
    {
        $this->shouldKeepAlive = $shouldKeepAlive;

        return $this;
    }

    public function hasBody(): bool
    {
        return isset($this->body) && $this->body->getSize() !== 0;
    }

    public function getBody(): StreamInterface
    {
        return $this->body ??= Psr7::createStream();
    }

    /** @Notice MUST clone the object before you change the body's content */
    public function setBody(mixed $body): static
    {
        $this->body = Psr7::createStreamFromAny($body);

        return $this;
    }

    public function withBody(StreamInterface $body): static
    {
        if ($body === $this->body) {
            return $this;
        }

        return (clone $this)->setBody($body);
    }

    public function toString(bool $withoutBody = false): string
    {
        throw new Error('This should be implemented in child classes');
    }
}
