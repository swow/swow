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

use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Message\UploadedFileInterface;

interface ServerRequestPlusInterface extends RequestPlusInterface, ServerRequestInterface
{
    /** @return array<string, mixed> */
    public function getServerParams(): array;

    /** @param array<string, mixed> $serverParams */
    public function setServerParams(array $serverParams): static;

    /** @param array<string, mixed> $serverParams */
    public function withServerParams(array $serverParams): static;

    /** @return array<string, string> */
    public function getQueryParams(): array;

    /** @param array<string, string> $query */
    public function setQueryParams(array $query): static;

    /** @param array<string, string> $query */
    public function withQueryParams(array $query): static;

    /** @return array<string, string> */
    public function getCookieParams(): array;

    /** @param array<string, string> $cookies */
    public function setCookieParams(array $cookies): static;

    /** @param array<string, string> $cookies */
    public function withCookieParams(array $cookies): static;

    /** @return array<mixed>|object|null */
    public function getParsedBody(): array|object|null;

    /** @param array<mixed>|object|null $data */
    public function setParsedBody(array|object|null $data): static;

    /** @param array<mixed>|object $data */
    public function withParsedBody($data): static;

    /** @return array<string, UploadedFileInterface> */
    public function getUploadedFiles(): array;

    /** @param array<string, UploadedFileInterface> $uploadedFiles */
    public function setUploadedFiles(array $uploadedFiles): static;

    /** @param array<UploadedFileInterface> $uploadedFiles */
    public function withUploadedFiles(array $uploadedFiles): static;

    /** @return array<mixed> */
    public function getAttributes(): array;

    /** @param string $name */
    public function getAttribute(mixed $name, mixed $default = null): mixed;

    public function setAttribute(string $name, mixed $value): static;

    public function unsetAttribute(string $name): static;

    /**
     * @param string $name
     */
    public function withAttribute(mixed $name, mixed $value): static;

    /** @param string $name */
    public function withoutAttribute($name): static;
}
