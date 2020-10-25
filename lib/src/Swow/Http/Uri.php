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

use Psr\Http\Message\UriInterface;

/**
 * PSR-7 URI implementation (https://github.com/Nyholm/psr7/blob/master/src/Uri.php)
 */
class Uri implements UriInterface
{
    protected const SCHEMES = ['http' => 80, 'https' => 443];

    /**
     * @var string uri scheme
     */
    protected $scheme = '';

    /**
     * @var string uri user info
     */
    protected $userInfo = '';

    /**
     * @var string uri host
     */
    protected $host = '';

    /**
     * @var null|int uri port
     */
    protected $port;

    /**
     * @var string uri path
     */
    protected $path = '';

    /**
     * @var string uri query string
     */
    protected $query = '';

    /**
     * @var string uri fragment
     */
    protected $fragment = '';

    public function __construct(string $uri = '')
    {
        if ($uri !== '') {
            $this->apply($uri);
        }
    }

    public function apply(string $uri): self
    {
        $parts = \parse_url($uri);
        if ($parts === false) {
            throw new \InvalidArgumentException("Unable to parse URI: {$uri}");
        }

        return $this->applyParts($parts);
    }

    public function applyParts(array $parts): self
    {
        $this->scheme = isset($parts['scheme']) ? strtolower($parts['scheme']) : '';
        $this->userInfo = $parts['user'] ?? '';
        $this->host = isset($parts['host']) ? strtolower($parts['host']) : '';
        $this->port = isset($parts['port']) ? $this->filterPort($parts['port']) : null;
        $this->path = isset($parts['path']) ? $this->filterPath($parts['path']) : '';
        $this->query = isset($parts['query']) ? $this->filterQueryAndFragment($parts['query']) : '';
        $this->fragment = isset($parts['fragment']) ? $this->filterQueryAndFragment($parts['fragment']) : '';
        if (isset($parts['pass'])) {
            $this->userInfo .= ':' . $parts['pass'];
        }

        return $this;
    }

    public function getScheme(): string
    {
        return $this->scheme;
    }

    public function setScheme(string $scheme): self
    {
        $scheme = strtolower($scheme);
        if ($scheme !== $this->scheme) {
            $this->scheme = $scheme;
            $this->port = $this->filterPort($this->port);
        }

        return $this;
    }

    public function filterScheme(string $scheme): string
    {
        return strtolower($scheme);
    }

    public function withScheme($scheme): self
    {
        $scheme = $this->filterScheme($scheme);
        if ($scheme === $this->scheme) {
            return $this;
        }

        $new = clone $this;
        $new->setScheme($scheme);

        return $new;
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

    public function setUserInfo(string $user, string $password = ''): self
    {
        $this->userInfo = $password === '' ? $user : "{$user}:{$password}";

        return $this;
    }

    public function filterUserInfo(string $user, string $password = ''): string
    {
        return $password === '' ? $user : "{$user}:{$password}";
    }

    public function withUserInfo($user, $password = ''): self
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

    public function setHost(string $host): self
    {
        $this->host = strtolower($host);

        return $this;
    }

    public function filterHost(string $host): string
    {
        return strtolower($host);
    }

    public function withHost($host): self
    {
        $host = $this->filterHost($host);
        if ($host === $this->host) {
            return $this;
        }

        $new = clone $this;
        $new->host = $host;

        return $new;
    }

    public function getPort(): ?int
    {
        return $this->port;
    }

    protected function filterPort(int $port): ?int
    {
        if ($port === null) {
            return null;
        }

        if ($port < 0 || $port > 0xffff) {
            throw new \InvalidArgumentException(\sprintf('Invalid port: %d. Must be between 0 and 65535', $port));
        }

        $scheme = $this->scheme;
        if (isset(static::SCHEMES[$scheme]) && $port === static::SCHEMES[$scheme]) {
            return null;
        }

        return $port;
    }

    public function setPort(?int $port): self
    {
        $this->port = $this->filterPort($port);

        return $this;
    }

    public function withPort($port): self
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
        return \preg_replace_callback(
            '/(?:[^a-zA-Z0-9_\-.~!\$&\'()*+,;=%:@\/]++|%(?![A-Fa-f0-9]{2}))/',
            static function (array $match): string {
                return \rawurlencode($match[0]);
            },
            $path
        );
    }

    public function setPath(string $path): self
    {
        $this->path = $this->filterPath($path);

        return $this;
    }

    public function withPath($path): self
    {
        $path = $this->filterPath($path);
        if ($path === $this->path) {
            return $this;
        }

        $new = clone $this;
        $new->path = $path;

        return $new;
    }

    public function getQuery(): string
    {
        return $this->query;
    }

    protected function filterQueryAndFragment(string $string): string
    {
        return \preg_replace_callback(
            '/(?:[^a-zA-Z0-9_\-.~!\$&\'()*+,;=%:@\/?]++|%(?![A-Fa-f0-9]{2}))/',
            static function (array $match): string {
                return \rawurlencode($match[0]);
            },
            $string
        );
    }

    public function setQuery(string $query): self
    {
        $this->query = $this->filterQueryAndFragment($query);

        return $this;
    }

    public function withQuery($query): self
    {
        $query = $this->filterQueryAndFragment($query);
        if ($query === $this->query) {
            return $this;
        }

        $new = clone $this;
        $new->query = $query;

        return $new;
    }

    public function getFragment(): string
    {
        return $this->fragment;
    }

    public function setFragment(string $fragment): self
    {
        $this->fragment = $this->filterQueryAndFragment($fragment);

        return $this;
    }

    public function withFragment($fragment): self
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
        $uri = '';
        if ($scheme !== '') {
            $uri .= $scheme . ':';
        }

        if ($authority !== '') {
            $uri .= '//' . $authority;
        }

        if ($path !== '') {
            if ($path[0] !== '/') {
                if ($authority !== '') {
                    // If the path is rootless and an authority is present, the path MUST be prefixed by "/"
                    $path = '/' . $path;
                }
            } elseif (isset($path[1]) && $path[1] === '/') {
                if ($authority === '') {
                    // If the path is starting with more than one "/" and no authority is present, the
                    // starting slashes MUST be reduced to one.
                    $path = '/' . \ltrim($path, '/');
                }
            }

            $uri .= $path;
        }

        if ($query !== '') {
            $uri .= '?' . $query;
        }

        if ($fragment !== '') {
            $uri .= '#' . $fragment;
        }

        return $uri;
    }

    public function toString(): string
    {
        return static::build($this->scheme, $this->getAuthority(), $this->path, $this->query, $this->fragment);
    }

    public function __toString(): string
    {
        return $this->toString();
    }
}
