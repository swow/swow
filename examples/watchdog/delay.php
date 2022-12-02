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
use Swow\Sync\WaitReference;
use Swow\Watchdog;

/* if CPU starvation occurred, kill the coroutine */
Watchdog::run(alerter: static function (): void { exit; });

$wf = new WaitReference();
Coroutine::run(static function () use ($wf): void {
    $count = 0;
    while (true) {
        $count++;
    }
});
WaitReference::wait($wf);
echo "Exited\n";
