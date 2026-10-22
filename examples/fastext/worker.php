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

require __DIR__ . '/autoload.php';

$action = $argv[1] ?? '';
$runDir = $argv[2] ?? '';
if ($action === '' || $runDir === '') {
    fwrite(STDERR, "usage: worker.php <archive|extract> <run directory>\n");
    exit(1);
}

if (!Swow\Examples\Fastext\Worker::runFromEnvironment($runDir, $action)) {
    exit(1);
}

Swow\Examples\Fastext\Worker::waitForStop();
