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

/* @noinspection PhpUnreachableStatementInspection */

declare(strict_types=1);

use Swow\Coroutine;
use Swow\WatchDog;

// 0.1s/t
WatchDog::run(100 * 1000 * 1000, 0, static function (): void {
    $coroutine = Coroutine::getCurrent();
    $coroutine->__alert_count = ($coroutine->__alert_count ?? 0) + 1;
    echo 'CPU starvation occurred, suspend this coroutine...' . PHP_EOL;
    sleep(0);
    if ($coroutine->__alert_count > 5) {
        echo 'Kill the bad guy' . PHP_EOL;
        $coroutine->kill();
    }
});

Coroutine::run(static function (): void {
    $s = microtime(true);
    for ($n = 5; $n--;) {
        echo 'Do something...' . PHP_EOL;
        usleep(1000);
    }
    $s = microtime(true) - $s;
    echo "Use {$s} ms" . PHP_EOL;
});

$count = 0;
try {
    while (true) {
        $count++;
    }
} finally {
    echo "Count is {$count}" . PHP_EOL;
}
