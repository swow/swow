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

    /**
     * @var array
     */
    protected $serverParams;

    /**
     * @var array
     */
    protected $cookieParams = [];

    /**
     * @var array
     */
    protected $queryParams = [];

    /**
     * @var null|array|object
     */
    protected $parsedBody;

    /**
     * @var UploadedFileInterface[]
     */
    protected $uploadedFiles = [];

    /**
     * @var array
     */
    protected $attributes = [];

    public function getServerParams(): array
    {
        return $this->serverParams;
    }

    /**
     * @return $this
     */
    public function setServerParams(array $serverParams)
    {
        $this->serverParams = $serverParams;

        return $this;
    }

    public function getQueryParams(): array
    {
        return $this->queryParams;
    }

    /**
     * @return $this
     */
    public function setQueryParams(array $query)
    {
        $this->queryParams = $query;

        return $this;
    }

    public function withQueryParams(array $query)
    {
        $new = clone $this;
        $new->queryParams = $query;

        return $new;
    }

    public function getCookieParams(): array
    {
        return $this->cookieParams;
    }

    public function setCookieParams(array $cookies)
    {
        $this->cookieParams = $cookies;

        return $this;
    }

    public function withCookieParams(array $cookies)
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
    public function setParsedBody($data)
    {
        $this->parsedBody = $data;

        return $this;
    }

    /**
     * @param array|object $data
     * @return $this
     */
    public function withParsedBody($data)
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
    public function setAttribute($attribute, $value)
    {
        $this->attributes[$attribute] = $value;

        return $this;
    }

    /**
     * @param string $attribute
     * @return $this
     */
    public function unsetAttribute($attribute)
    {
        unset($this->attributes[$attribute]);

        return $this;
    }

    /**
     * @param string $attribute
     * @param mixed $value
     * @return $this
     */
    public function withAttribute($attribute, $value)
    {
        $new = clone $this;
        $new->attributes[$attribute] = $value;

        return $new;
    }

    /**
     * @param string $attribute
     * @return $this
     */
    public function withoutAttribute($attribute)
    {
        if (array_key_exists($attribute, $this->attributes) === false) {
            return $this;
        }

        $new = clone $this;
        unset($new->attributes[$attribute]);

        return $new;
    }
}
