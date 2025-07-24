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

require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

printf("child process started\n");

$f = fopen($argv[1], 'r');
if ($f === false) {
    printf("failed to open fifo\n");
    exit(1);
}

Coroutine::run(static function () use ($f): void {
    printf("read-end read\n");
    $read = fread($f, 5);
    printf("read-end read: %s\n", $read);
});

// if coroutine swap happend (which means fread hooked)
// this will be shown before "child process read"
printf("Done\n");
