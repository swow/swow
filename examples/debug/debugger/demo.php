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

require __DIR__ . '/../../../tools/autoload.php';

use Swow\Channel;
use Swow\Coroutine;
use Swow\Debug\Debugger;

Debugger::runOnTTY();

for ($n = 1; $n <= 3; $n++) {
    Coroutine::run(function () use ($n) {
        $tip = "I am zombie {$n}";
        if (mt_rand(0, 1)) {
            Coroutine::yield();
        } else {
            (new Channel())->pop();
        }
        unset($tip);
    });
}
for ($n = 1; $n < 10; $n++) {
    Coroutine::run(function () use ($n) {
        $count = 1;
        while ($count) {
            $c = function () use ($n, &$count) {
                $tip = 'I am C';
                usleep($n * 111111);
                unset($tip);
            };
            $b = function () use ($c) {
                $tip = 'I am B';
                $c();
                unset($tip);
            };
            $a = function () use ($b) {
                $tip = 'I am A';
                $b();
                unset($tip);
            };
            $a();
            $count++;
        }
    });
}
