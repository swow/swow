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
use function parse_str;

class ServerRequest extends Request implements ServerRequestInterface
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
    protected ?array $queryParams = null;

    /** @var array<mixed>|object */
    protected array|object $parsedBody;

    protected ?int $contentLength = null;

    protected ?string $contentType = null;

    protected ?int $upgrade = null;

    /**
     * @var UploadedFileInterface[]
     */
    protected array $uploadedFiles = [];

    /** @var array<string, mixed> */
    protected array $attributes = [];

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

    /** @return array<string, string> */
    public function getQueryParams(): array
    {
        return $this->queryParams ??= $this->getUri()->getQueryParams();
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

    public function getUpgrade(): int
    {
        return $this->upgrade ??= parent::getUpgrade();
    }

    /** @return array<mixed>|object */
    public function getParsedBody(): array|object
    {
        return $this->parsedBody ??= BodyParser::parse($this->getBody(), $this->getContentType());
    }

    /** @param array<mixed>|object $data */
    public function setParsedBody(array|object $data): static
    {
        $this->parsedBody = $data;

        return $this;
    }

    /**
     * @param array<mixed>|object $data
     */
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
    public function withUploadedFiles(array $uploadedFiles)
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
     * @return mixed
     */
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

    /**
     * @param string $name
     * @param mixed $value
     */
    public function withAttribute($name, $value): static
    {
        return (clone $this)->setAttribute($name, $value);
    }

    public function unsetAttribute(string $name): static
    {
        unset($this->attributes[$name]);

        return $this;
    }

    /**
     * @param string $name
     */
    public function withoutAttribute($name): static
    {
        if (array_key_exists($name, $this->attributes) === false) {
            return $this;
        }

        return (clone $this)->unsetAttribute($name);
    }

    /**
     * @param array<string, array<string>> $headers
     * @param array<string, string> $serverParams
     */
    public function constructFromRawRequest(RawRequest $request): static
    {
        $this->protocolVersion = $request->protocolVersion;
        $this->uri = Uri::from($request->uri);
        $this->method = $request->method;
        $this->headers = $request->headers;
        $this->headerNames = $request->headerNames;
        $this->keepAlive = $request->shouldKeepAlive;
        $this->contentLength = $request->contentLength;
        if (!$request->isUpgrade) {
            $this->upgrade = static::UPGRADE_NONE;
        }
        $this->serverParams = $request->serverParams;
        if ($this->hasHeader('cookie')) {
            // FIXME: it this reliable?
            parse_str(
                strtr($this->getHeaderLine('cookie'), ['&' => '%26', '+' => '%2B', ';' => '&']),
                $this->cookieParams
            );
        }
        if ($request->body) {
            $this->setBody($request->body);
        }
        if ($request->formData) {
            $this->setParsedBody($request->formData);
        }
        if ($request->isMultipart) {
            // TODO
        }
        if ($request->uploadedFiles) {
            $uploadedFiles = [];
            foreach ($request->uploadedFiles as $formDataName => $rawUploadedFile) {
                $uploadedFiles[$formDataName] = new UploadedFile(
                    $rawUploadedFile->tmp_name,
                    $rawUploadedFile->size,
                    $rawUploadedFile->error,
                    $rawUploadedFile->name,
                    $rawUploadedFile->type
                );
            }
            $this->setUploadedFiles($uploadedFiles);
        }

        return $this;
    }
}
