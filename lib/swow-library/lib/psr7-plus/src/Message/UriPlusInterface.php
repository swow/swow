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
    public function setScheme(string $scheme): static;

    public function setUserInfo(string $user, string $password = ''): static;

    public function setHost(string $host): static;

    public function setPort(?int $port): static;

    public function setPath(string $path): static;

    public function setQuery(string $query): static;

    /** @return array<string, string> */
    public function getQueryParams(): array;

    /** @param array<string, string> $queryParams */
    public function setQueryParams(array $queryParams): static;

    /** @param array<string, string> $queryParams */
    public function withQueryParams(array $queryParams): static;

    public function setFragment(string $fragment): static;

    public function toString(): string;
}
