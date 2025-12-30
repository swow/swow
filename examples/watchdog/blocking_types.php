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
use Swow\Debug;
use Swow\Sync\WaitReference;
use Swow\Watchdog;

/* alerter receives blocking type: 'cpu' or 'syscall' */
Watchdog::run(
    quantum: 100 * 1000 * 1000, // 0.1s
    threshold: 300 * 1000 * 1000, // 0.3s
    alerter: static function (string $blockingType): void {
        $coroutine = Coroutine::getCurrent();
        echo "Watchdog alert: blocking_type={$blockingType}, coroutine={$coroutine->getId()}" . PHP_EOL;
        if ($blockingType === 'cpu') {
            sleep(0); // yield to other coroutines
        } elseif ($blockingType === 'syscall') {
            // print backtrace when syscall blocking detected
            echo Coroutine::getCurrent()->getTraceAsString(), "\n";
        }
    }
);

$wf = new WaitReference();

echo '--- Demo 1: CPU blocking ---' . PHP_EOL;
Coroutine::run(static function () use ($wf): void {
    $count = 0;
    $start = microtime(true);
    while (microtime(true) - $start < 0.25) {
        $count++;
    }
    echo "CPU task done, count={$count}" . PHP_EOL;
});

sleep(1);

echo '--- Demo 2: Syscall blocking (via Debug\block) ---' . PHP_EOL;
Coroutine::run(static function () use ($wf): void {
    echo 'Calling Debug\block(1000)...' . PHP_EOL;
    Debug\block(1000); // Block for 1 second (1000 milliseconds)
    echo 'Block returned' . PHP_EOL;
});

WaitReference::wait($wf);
echo 'Done' . PHP_EOL;
