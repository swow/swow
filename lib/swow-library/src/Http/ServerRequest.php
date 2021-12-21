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
use function array_key_exists;
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

    /**
     * @var null|array|object
     */
    protected $parsedBody;

    /**
     * @var UploadedFileInterface[]
     */
    protected array $uploadedFiles = [];

    protected array $attributes = [];

    public function getServerParams(): array
    {
        return $this->serverParams;
    }

    /** @return $this */
    public function setServerParams(array $serverParams): static
    {
        $this->serverParams = $serverParams;

        return $this;
    }

    public function getQueryParams(): array
    {
        return $this->queryParams;
    }

    /** @return $this */
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

    /** @return $this */
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

    public function getParsedBody()
    {
        if ($this->parsedBody) {
            return $this->parsedBody;
        }

        return $this->parsedBody = BodyParser::parse($this);
    }

    /**
     * @param array|object $data
     * @return $this
     */
    public function setParsedBody($data): static
    {
        $this->parsedBody = $data;

        return $this;
    }

    /**
     * @param array|object $data
     */
    public function withParsedBody($data): static
    {
        $new = clone $this;
        $new->parsedBody = $data;

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

    public function getAttribute($attribute, $default = null)
    {
        if (array_key_exists($attribute, $this->attributes) === false) {
            return $default;
        }

        return $this->attributes[$attribute];
    }

    /**
     * @param mixed $attribute
     * @param mixed $value
     * @return $this
     */
    public function setAttribute($attribute, $value): static
    {
        $this->attributes[$attribute] = $value;

        return $this;
    }

    /**
     * @param string $attribute
     * @return $this
     */
    public function unsetAttribute($attribute): static
    {
        unset($this->attributes[$attribute]);

        return $this;
    }

    /**
     * @param string $attribute
     * @param mixed $value
     */
    public function withAttribute($attribute, $value): static
    {
        $new = clone $this;
        $new->attributes[$attribute] = $value;

        return $new;
    }

    /**
     * @param string $attribute
     */
    public function withoutAttribute($attribute): static
    {
        if (array_key_exists($attribute, $this->attributes) === false) {
            return $this;
        }

        $new = clone $this;
        unset($new->attributes[$attribute]);

        return $new;
    }
}
