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

namespace Swow\StubUtils\ConstantFixer;

use function array_key_exists;
use function Swow\Util\httpGet;

abstract class ConstantDefinitionsFetcherAbstract implements ConstantDefinitionsFetcher
{
    /** @var array<string, string> */
    protected static array $httpCache = [];

    protected function httpGet(string $uri): string
    {
        if (array_key_exists($uri, static::$httpCache)) {
            //printf("cache hit %s\n", $uri);
            return static::$httpCache[$uri];
        }
        //printf("cache miss %s\n", $uri);
        return static::$httpCache[$uri] = httpGet($uri);
    }
}
