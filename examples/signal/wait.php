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

use Swow\Signal;

$pid = getmypid();
$count = 3;

do {
    echo sprintf("Press [CTRL + C] %s times if you want to quit\n", $count--);
    Signal::wait(Signal::INT);
    echo "\n"; // for ^C
} while ($count);

echo "Quit\n";
