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

namespace Swow\Context;

use Swow\Coroutine;
use WeakMap;

final class CoroutineContext
{
    /** @var WeakMap<Coroutine, Context> */
    private static WeakMap $contextMap;

    public static function get(?Coroutine $coroutine = null): Context
    {
        $contextMap = self::$contextMap ??= new WeakMap();
        return $contextMap[$coroutine ?? Coroutine::getCurrent()] ??= new Context();
    }
}
