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

namespace Swow\Http\Server;

use function explode;
use function parse_str;
use function rawurldecode;

class Request extends \Swow\Http\ServerRequest
{
    protected string $path = '';

    protected string $query = '';

    protected array $queryParams;

    protected bool $upgrade;

    protected ?int $contentLength = null;

    /** @noinspection PhpMissingParentConstructorInspection */
    public function __construct()
    {
        // do not construct it, it will be constructed by Server
    }

    public function getPath(): string
    {
        return $this->path;
    }

    public function getQuery(): string
    {
        return $this->query;
    }

    public function getQueryParams(): array
    {
        if ($this->queryParams === null) {
            parse_str($this->query, $this->queryParams);
        }

        return $this->queryParams;
    }

    public function getUpgrade(): int
    {
        if ($this->upgrade === false) {
            return static::UPGRADE_NONE;
        }

        return parent::getUpgrade();
    }

    /**
     * @return $this
     */
    public function setUpgrade(bool $upgrade)
    {
        $this->upgrade = $upgrade;

        return $this;
    }

    public function getContentLength(): int
    {
        if ($this->contentLength !== null) {
            /* @Notice Used when we do not want to recv body */
            return $this->contentLength;
        }

        return parent::getContentLength();
    }

    /**
     * @return $this
     */
    public function setContentLength(int $contentLength)
    {
        $this->contentLength = $contentLength;

        return $this;
    }

    public function setHead(string $method, string $uri, string $protocolVersion, array $headers, array $headerNames, bool $keepAlive, int $contentLength, bool $upgrade)
    {
        $this->method = $method;
        $this->uriString = $uri;
        $uriParts = explode('?', $uri, 2);
        $this->path = rawurldecode($uriParts[0]);
        $this->query = $uriParts[1] ?? '';
        $this->protocolVersion = $protocolVersion;
        $this->headers = $headers;
        $this->headerNames = $headerNames;
        $this->keepAlive = $keepAlive;
        $this->contentLength = $contentLength;
        $this->upgrade = $upgrade;

        return $this;
    }
}
