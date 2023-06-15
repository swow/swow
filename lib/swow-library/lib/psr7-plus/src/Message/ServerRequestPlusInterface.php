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
    /** @param array<string, mixed> $serverParams */
    public function setServerParams(array $serverParams): static;

    /** @param array<string, mixed> $serverParams */
    public function withServerParams(array $serverParams): static;

    /** @param array<string, string> $query */
    public function setQueryParams(array $query): static;

    /** @param array<string, string> $cookies */
    public function setCookieParams(array $cookies): static;

    /** @param array<mixed>|object $data */
    public function setParsedBody(array|object $data): static;

    /** @param array<string, UploadedFileInterface> $uploadedFiles */
    public function setUploadedFiles(array $uploadedFiles): static;

    public function setAttribute(string $name, mixed $value): static;

    public function unsetAttribute(string $name): static;
}
