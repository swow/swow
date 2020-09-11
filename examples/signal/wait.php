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

use Swow\Signal;

$pid = getmypid();
$count = 3;

echo "Press Ctrl + C\n";

do {
    Signal::wait(Signal::INT);
    echo "\n"; // for ^C
} while ($count-- && print_r("Repeat {$count} times if you want to quit\n"));

echo "Quit\n";
