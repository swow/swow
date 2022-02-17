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

use Swow\Coroutine;

$coroutine = Coroutine::run(static function (): void {
    while (true) {
        try {
            // output: int(999)
            var_dump(sleep(999));
        } catch (Exception) {
            // skip
        }
    }
});
$coroutine->resume(); // resume the coroutine to cancel the sleep
$coroutine->throw(new Exception()); // throw exception to the coroutine
$coroutine->kill(); // kill the coroutine (just like SIGKILL)
