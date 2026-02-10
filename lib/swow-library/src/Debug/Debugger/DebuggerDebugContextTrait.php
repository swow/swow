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
/**
 * Author: Twosee <twose@qq.com>
 * Date: 2024/3/24 03:48
 */

namespace Swow\Debug\Debugger;

use Swow\Coroutine;
use WeakMap;

trait DebuggerDebugContextTrait
{
    /** @var WeakMap<Coroutine, DebugContext> */
    protected static WeakMap $coroutineDebugWeakMap;

    public static function getDebugContextOfCoroutine(Coroutine $coroutine): DebugContext
    {
        $coroutineDebugWeakMap = static::$coroutineDebugWeakMap ?? (static::$coroutineDebugWeakMap = new WeakMap());
        return $coroutineDebugWeakMap[$coroutine] ??= new DebugContext();
    }
}
