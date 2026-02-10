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

use function Swow\Debug\showExecutedSourceLines;

require __DIR__ . '/../autoload.php';

showExecutedSourceLines();

while (true) {
    echo date('Y-m-d H:i:s') . "\n";
    sleep(1);
}
