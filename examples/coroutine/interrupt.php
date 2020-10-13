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

use Swow\Coroutine;

$coroutine = Coroutine::run(function () {
    while (true) {
        try {
            var_dump(sleep(999));
        } catch (Exception $e) {
            // 忽略
        }
    }
});
$coroutine->resume(); // 强制唤醒中断睡眠
$coroutine->throw(new Exception()); // 向协程抛出异常
$coroutine->kill(); // 杀死协程
