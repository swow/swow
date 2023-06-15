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

use Psr\Http\Message\UploadedFileInterface;

use function array_key_exists;

class ServerRequest extends Request implements ServerRequestPlusInterface
{
    /**
     * @var bool Keep the Host header (The server may verify its legitimacy)
     */
    protected const PRESERVE_HOST = true;

    /** @var array<string, mixed> */
    protected array $serverParams = [];

    /** @var array<string, string> */
    protected array $cookieParams = [];

    /** @var array<string, string> */
    protected array $queryParams = [];

    /** @var array<mixed>|object|null */
    protected array|object|null $parsedBody;

    protected ?int $contentLength = null;

    protected ?string $contentType = null;

    protected ?bool $isUpgrade = null;

    /**
     * @var UploadedFileInterface[]
     */
    protected array $uploadedFiles = [];

    /** @var array<string, mixed> */
    protected array $attributes = [];

    final public function __construct()
    {
    }

    /** @return array<string, mixed> */
    public function getServerParams(): array
    {
        return $this->serverParams;
    }

    /** @param array<string, mixed> $serverParams */
    public function setServerParams(array $serverParams): static
    {
        $this->serverParams = $serverParams;

        return $this;
    }

    /** @param array<string, mixed> $serverParams */
    public function withServerParams(array $serverParams): static
    {
        $new = clone $this;
        $new->serverParams = $serverParams;

        return $new;
    }

    /** @return array<string, string> */
    public function getQueryParams(): array
    {
        return $this->queryParams;
    }

    /** @param array<string, string> $query */
    public function setQueryParams(array $query): static
    {
        $this->queryParams = $query;

        return $this;
    }

    /** @param array<string, string> $query */
    public function withQueryParams(array $query): static
    {
        $new = clone $this;
        $new->queryParams = $query;

        return $new;
    }

    /** @return array<string, string> */
    public function getCookieParams(): array
    {
        return $this->cookieParams;
    }

    /** @param array<string, string> $cookies */
    public function setCookieParams(array $cookies): static
    {
        $this->cookieParams = $cookies;

        return $this;
    }

    /** @param array<string, string> $cookies */
    public function withCookieParams(array $cookies): static
    {
        $new = clone $this;
        $new->cookieParams = $cookies;

        return $new;
    }

    public function getContentLength(): int
    {
        return $this->contentLength ??= parent::getContentLength();
    }

    public function getContentType(): string
    {
        return $this->contentType ??= parent::getContentType();
    }

    public function isUpgrade(): ?bool
    {
        return $this->isUpgrade;
    }

    public function setIsUpgrade(bool $isUpgrade): static
    {
        $this->isUpgrade = $isUpgrade;

        return $this;
    }

    /** @return array<mixed>|object */
    public function getParsedBody(): array|object
    {
        return $this->parsedBody ??= BodyDecoder::decode($this->getBody(), $this->getContentType());
    }

    /** @param array<mixed>|object|null $data */
    public function setParsedBody(array|object|null $data): static
    {
        $this->parsedBody = $data;

        return $this;
    }

    /** @param array<mixed>|object $data */
    public function withParsedBody($data): static
    {
        return (clone $this)->setParsedBody($data);
    }

    /** @return array<string, UploadedFileInterface> */
    public function getUploadedFiles(): array
    {
        return $this->uploadedFiles;
    }

    /** @param array<string, UploadedFileInterface> $uploadedFiles */
    public function setUploadedFiles(array $uploadedFiles): static
    {
        $this->uploadedFiles = $uploadedFiles;

        return $this;
    }

    /** @param array<UploadedFileInterface> $uploadedFiles */
    public function withUploadedFiles(array $uploadedFiles): static
    {
        $new = clone $this;
        $new->uploadedFiles = $uploadedFiles;

        return $new;
    }

    /** @return array<mixed> */
    public function getAttributes(): array
    {
        return $this->attributes;
    }

    /**
     * @param string $name
     * @param mixed $default
     */
    public function getAttribute(mixed $name, mixed $default = null): mixed
    {
        if (array_key_exists($name, $this->attributes) === false) {
            return $default;
        }

        return $this->attributes[$name];
    }

    public function setAttribute(string $name, mixed $value): static
    {
        $this->attributes[$name] = $value;

        return $this;
    }

    /**
     * @param string $name
     */
    public function withAttribute(mixed $name, mixed $value): static
    {
        return (clone $this)->setAttribute($name, $value);
    }

    public function unsetAttribute(string $name): static
    {
        unset($this->attributes[$name]);

        return $this;
    }

    /** @param string $name */
    public function withoutAttribute($name): static
    {
        if (array_key_exists($name, $this->attributes) === false) {
            return $this;
        }

        return (clone $this)->unsetAttribute($name);
    }
}
