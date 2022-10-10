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

use InvalidArgumentException;
use Swow\Object\StringableInterface;
use Swow\Object\StringableTrait;

use function http_build_query;
use function parse_str;
use function parse_url;
use function preg_replace_callback;
use function rawurlencode;
use function sprintf;
use function str_starts_with;
use function strtolower;

/**
 * PSR-7 URI implementation (https://github.com/Nyholm/psr7/blob/master/src/Uri.php)
 */
class Uri implements UriPlusInterface, StringableInterface
{
    use StringableTrait;

    protected const SCHEMES = ['http' => 80, 'https' => 443];

    protected string $scheme = '';

    protected string $userInfo = '';

    protected string $host = '';

    protected ?int $port = null;

    protected string $path = '';

    protected string $query = '';

    /** @var array<string, string>|null */
    protected ?array $queryParams = null;

    protected string $fragment = '';

    public function __construct(string $uri = '')
    {
        if ($uri !== '') {
            $this->apply($uri);
        }
    }

    public function apply(string $uri): static
    {
        /**
         * @var array<string, string>|false $parts
         */
        $parts = parse_url($uri);
        if ($parts === false) {
            throw new InvalidArgumentException("Unable to parse URI: {$uri}");
        }

        return $this->applyParts($parts);
    }

    /**
     * @param array<string, string> $parts
     */
    public function applyParts(array $parts): static
    {
        $this->scheme = isset($parts['scheme']) ? strtolower($parts['scheme']) : '';
        $this->userInfo = $parts['user'] ?? '';
        $this->host = isset($parts['host']) ? strtolower($parts['host']) : '';
        $this->port = isset($parts['port']) ? $this->filterPort((int) $parts['port']) : null;
        $this->path = isset($parts['path']) ? $this->filterPath($parts['path']) : '';
        $this->query = isset($parts['query']) ? $this->filterQueryAndFragment($parts['query']) : '';
        $this->fragment = isset($parts['fragment']) ? $this->filterQueryAndFragment($parts['fragment']) : '';
        if (isset($parts['pass'])) {
            $this->userInfo .= ':' . $parts['pass'];
        }

        return $this;
    }

    protected function filterSchemeAndHost(string $host): string
    {
        return strtolower($host);
    }

    public function getScheme(): string
    {
        return $this->scheme;
    }

    public function setScheme(string $scheme): static
    {
        $scheme = $this->filterSchemeAndHost($scheme);
        if ($scheme !== $this->scheme) {
            $this->scheme = $scheme;
            $this->port = $this->filterPort($this->port);
        }

        return $this;
    }

    /** @param string $scheme */
    public function withScheme(mixed $scheme): static
    {
        $scheme = $this->filterSchemeAndHost($scheme);
        if ($scheme === $this->scheme) {
            return $this;
        }

        return (clone $this)->setScheme($scheme);
    }

    public function getAuthority(): string
    {
        if ($this->host === '') {
            return '';
        }

        $authority = $this->host;
        if ($this->userInfo !== '') {
            $authority = $this->userInfo . '@' . $authority;
        }

        if ($this->port !== null) {
            $authority .= ':' . $this->port;
        }

        return $authority;
    }

    public function getUserInfo(): string
    {
        return $this->userInfo;
    }

    protected function filterUserInfo(string $user, string $password = ''): string
    {
        return $password === '' ? $user : "{$user}:{$password}";
    }

    public function setUserInfo(string $user, string $password = ''): static
    {
        $this->userInfo = $this->filterUserInfo($user, $password);

        return $this;
    }

    /**
     * @param string $user
     * @param string $password
     */
    public function withUserInfo(mixed $user, mixed $password = ''): static
    {
        $userInfo = $this->filterUserInfo($user, $password);
        if ($userInfo === $this->userInfo) {
            return $this;
        }

        $new = clone $this;
        $new->userInfo = $userInfo;

        return $new;
    }

    public function getHost(): string
    {
        return $this->host;
    }

    public function setHost(string $host): static
    {
        $this->host = $this->filterSchemeAndHost($host);

        return $this;
    }

    /** @param string $host */
    public function withHost(mixed $host): static
    {
        $host = $this->filterSchemeAndHost($host);
        if ($host === $this->host) {
            return $this;
        }

        return (clone $this)->setHost($host);
    }

    public function getPort(): ?int
    {
        return $this->port;
    }

    protected function filterPort(?int $port): ?int
    {
        if ($port === null) {
            return null;
        }

        if ($port < 0 || $port > 0xFFFF) {
            throw new InvalidArgumentException(sprintf('Invalid port: %d. Must be between 0 and 65535', $port));
        }

        $scheme = $this->scheme;
        if (isset(static::SCHEMES[$scheme]) && $port === static::SCHEMES[$scheme]) {
            return null;
        }

        return $port;
    }

    public function setPort(?int $port): static
    {
        $this->port = $this->filterPort($port);

        return $this;
    }

    /** @param int $port */
    public function withPort($port): static
    {
        $port = $this->filterPort($port);
        if ($port === $this->port) {
            return $this;
        }

        $new = clone $this;
        $new->port = $port;

        return $new;
    }

    public function getPath(): string
    {
        return $this->path;
    }

    protected function filterPath(string $path): string
    {
        return preg_replace_callback(
            '/[^a-zA-Z0-9_\-.~!&\'()*+,;=%:@\/]++|%(?![A-Fa-f0-9]{2})/',
            static fn (array $match): string => rawurlencode($match[0]),
            $path
        );
    }

    public function setPath(string $path): static
    {
        $this->path = $this->filterPath($path);

        return $this;
    }

    /** @param string $path */
    public function withPath(mixed $path): static
    {
        $path = $this->filterPath($path);
        if ($path === $this->path) {
            return $this;
        }

        $new = clone $this;
        $new->path = $path;

        return $new;
    }

    protected function filterQueryAndFragment(string $string): string
    {
        return preg_replace_callback(
            '/[^a-zA-Z0-9_\-.~!&\'()*+,;=%:@\/?]++|%(?![A-Fa-f0-9]{2})/',
            static fn (array $match): string => rawurlencode($match[0]),
            $string
        );
    }

    public function getQuery(): string
    {
        return $this->query;
    }

    public function setQuery(string $query): static
    {
        $this->query = $this->filterQueryAndFragment($query);
        $this->queryParams = null;

        return $this;
    }

    /** @param string $query */
    public function withQuery(mixed $query): static
    {
        $query = $this->filterQueryAndFragment($query);
        if ($query === $this->query) {
            return $this;
        }

        $new = clone $this;
        $new->query = $query;
        $new->queryParams = null;

        return $new;
    }

    /** @return array<string, string> */
    public function getQueryParams(): array
    {
        if (!isset($this->queryParams)) {
            $query = $this->query;
            if ($query === '') {
                $this->queryParams = [];
            } else {
                parse_str($query, $this->queryParams);
            }
        }

        return $this->queryParams;
    }

    public function setQueryParams(array $queryParams): static
    {
        $this->query = http_build_query($queryParams);
        $this->queryParams = $queryParams;

        return $this;
    }

    public function withQueryParams(array $queryParams): static
    {
        return (clone $this)->setQueryParams($queryParams);
    }

    public function getFragment(): string
    {
        return $this->fragment;
    }

    public function setFragment(string $fragment): static
    {
        $this->fragment = $this->filterQueryAndFragment($fragment);

        return $this;
    }

    /** @param string $fragment */
    public function withFragment(mixed $fragment): static
    {
        $fragment = $this->filterQueryAndFragment($fragment);
        if ($fragment === $this->fragment) {
            return $this;
        }

        $new = clone $this;
        $new->fragment = $fragment;

        return $new;
    }

    public static function build(string $scheme, string $authority, string $path, string $query, string $fragment): string
    {
        $schemeSuffix = $scheme !== '' ? ':' : '';
        $authorityPrefix = $authority !== '' ? '//' : '';
        $pathPrefix = '';
        if ($path !== '' && !str_starts_with($path, '/') && $authority !== '') {
            // If the path is rootless and an authority is present, the path MUST be prefixed by "/"
            $pathPrefix = '/';
        }
        $queryPrefix = $query !== '' ? '?' : '';
        $fragmentPrefix = $fragment !== '' ? '#' : '';

        return $scheme . $schemeSuffix . $authorityPrefix . $authority . $pathPrefix . $path . $queryPrefix . $query . $fragmentPrefix . $fragment;
    }

    public function toString(): string
    {
        return static::build($this->scheme, $this->getAuthority(), $this->path, $this->query, $this->fragment);
    }
}
