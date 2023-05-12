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

use Psr\Http\Message\UriInterface;

interface UriPlusInterface extends UriInterface
{
    public function getScheme(): string;

    public function setScheme(string $scheme): static;

    /** @param string $scheme */
    public function withScheme(mixed $scheme): static;

    public function getAuthority(): string;

    public function getUserInfo(): string;

    public function setUserInfo(string $user, string $password = ''): static;

    /**
     * @param string $user
     * @param string $password
     */
    public function withUserInfo(mixed $user, mixed $password = ''): static;

    public function getHost(): string;

    public function setHost(string $host): static;

    /** @param string $host */
    public function withHost(mixed $host): static;

    public function getPort(): ?int;

    public function setPort(?int $port): static;

    /** @param int $port */
    public function withPort($port): static;

    public function getPath(): string;

    public function setPath(string $path): static;

    /** @param string $path */
    public function withPath(mixed $path): static;

    public function getQuery(): string;

    public function setQuery(string $query): static;

    /** @param string $query */
    public function withQuery(mixed $query): static;

    /** @return array<string, string> */
    public function getQueryParams(): array;

    /** @param array<string, string> $queryParams */
    public function setQueryParams(array $queryParams): static;

    /** @param array<string, string> $queryParams */
    public function withQueryParams(array $queryParams): static;

    public function getFragment(): string;

    public function setFragment(string $fragment): static;

    /** @param string $fragment */
    public function withFragment(mixed $fragment): static;

    public function toString(): string;
}
