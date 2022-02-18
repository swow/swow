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

use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Message\UploadedFileInterface;
use RuntimeException;
use function array_key_exists;
use function implode;
use function preg_match;
use function strcasecmp;

class ServerRequest extends Request implements ServerRequestInterface
{
    public const UPGRADE_NONE = 0;
    public const UPGRADE_WEBSOCKET = 1 << 0;
    public const UPGRADE_H2C = 1 << 1;
    public const UPGRADE_UNKNOWN = 1 << 31;

    /**
     * @var bool Keep the Host header (The server may verifies its legitimacy)
     */
    protected const PRESERVE_HOST = true;

    protected array $serverParams = [];

    protected array $cookieParams = [];

    protected array $queryParams = [];

    protected null|array|object $parsedBody;

    /**
     * @var UploadedFileInterface[]
     */
    protected array $uploadedFiles = [];

    protected array $attributes = [];

    public function getServerParams(): array
    {
        return $this->serverParams;
    }

    public function setServerParams(array $serverParams): static
    {
        $this->serverParams = $serverParams;

        return $this;
    }

    public function getQueryParams(): array
    {
        return $this->queryParams;
    }

    public function setQueryParams(array $query): static
    {
        $this->queryParams = $query;

        return $this;
    }

    public function withQueryParams(array $query): static
    {
        $new = clone $this;
        $new->queryParams = $query;

        return $new;
    }

    public function getCookieParams(): array
    {
        return $this->cookieParams;
    }

    public function setCookieParams(array $cookies): static
    {
        $this->cookieParams = $cookies;

        return $this;
    }

    public function withCookieParams(array $cookies): static
    {
        $new = clone $this;
        $new->cookieParams = $cookies;

        return $new;
    }

    public function getUpgrade(): int
    {
        $upgrade = $this->getHeaderLine('upgrade');
        if ($upgrade === '') {
            return static::UPGRADE_NONE;
        }
        if (strcasecmp($upgrade, 'websocket') === 0) {
            return static::UPGRADE_WEBSOCKET;
        }
        if (strcasecmp($upgrade, 'h2c') === 0) {
            return static::UPGRADE_H2C;
        }

        return static::UPGRADE_UNKNOWN;
    }

    public function getParsedBody(): null|array|object
    {
        return $this->parsedBody ??= BodyParser::parse($this);
    }

    public function setParsedBody(null|array|object $data): static
    {
        $this->parsedBody = $data;

        return $this;
    }

    /**
     * @param array|object|null $data
     */
    public function withParsedBody($data): static
    {
        $new = clone $this;
        $new->setParsedBody($data);

        return $new;
    }

    public function getUploadedFiles(): array
    {
        return $this->uploadedFiles;
    }

    public function setUploadedFiles(array $uploadedFiles)
    {
        $this->uploadedFiles = $uploadedFiles;

        return $this;
    }

    public function withUploadedFiles(array $uploadedFiles)
    {
        $new = clone $this;
        $new->uploadedFiles = $uploadedFiles;

        return $new;
    }

    public function getAttributes(): array
    {
        return $this->attributes;
    }

    public function getAttribute($name, $default = null)
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

    public function unsetAttribute(string $name): static
    {
        unset($this->attributes[$name]);

        return $this;
    }

    /**
     * @param string $name
     * @param mixed $value
     */
    public function withAttribute($name, $value): static
    {
        $new = clone $this;
        $new->setAttribute($name, $value);

        return $new;
    }

    /**
     * @param string $name
     */
    public function withoutAttribute($name): static
    {
        if (array_key_exists($name, $this->attributes) === false) {
            return $this;
        }

        $new = clone $this;
        $new->unsetAttribute($name);

        return $new;
    }

    public function toStringEx(bool $headOnly = false, ?string $body = null): string
    {
        if (!$headOnly) {
            $uploadedFiles = $this->getUploadedFiles();
            if ($uploadedFiles !== [] && $this->getBody()->isEmpty()) {
                $contentType = $this->getHeaderLine('content-type');
                if (!preg_match('/boundary(?: *)?=(?: *)?\"?([a-zA-Z0-9\'\(\)+_,-.\/:=? ]*)(?<! )\"?/', $contentType, $matches)) {
                    throw new RuntimeException('Can not find boundary in Content-Type header');
                }
                $boundary = $matches[1];
                $parts = [];
                foreach ($uploadedFiles as $uploadedFile) {
                    $stream = $uploadedFile->getStream();
                    $parts[] = "--{$boundary}\r\nContent-Disposition: form-data; name=\"{$uploadedFile->getClientFilename()}\"\r\n\r\n{$stream->getContents()}\r\n";
                }
                $parts[] = "--{$boundary}--\r\n";
                $body = implode('', $parts);
            }
        }
        return parent::toStringEx($headOnly, $body);
    }
}
