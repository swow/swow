#!/usr/bin/env php
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

use function Swow\Tools\br;
use function Swow\Tools\notice;
use function Swow\Tools\passthru;
use function Swow\Tools\warn;

require __DIR__ . '/../tools.php';

$NOLIMIT = 8192;
$workspace = __DIR__ . '/../../ext';

if ('Windows' === PHP_OS_FAMILY) {
    // No need to set nolimit on Windows
    $shell = 'CMD /C';
    $cmd = PHP_BINARY;
} else {
    $shell = 'sh -c';
    $cmd = "{ ulimit -n {$NOLIMIT} || echo 'Cannot set nolimit to {$NOLIMIT}, some tests may fail.' ; } && NO_INTERACTION=1 " . PHP_BINARY;
}

if (!extension_loaded(\Swow::class)) {
    $enable_swow = '-d extension=swow';
    $check = '-d extension=swow --ri swow';
    $status = passthru(sprintf('%s "%s %s"', $shell, $cmd, $check));
    br();
    if ($status !== 0) {
        warn('Failed load swow extension, have you installed swow (i.e. run composer build-extension)?');
        exit($status);
    }
} else {
    $enable_swow = '';
}
$options = "-n {$workspace}/tests/runner/run-tests.php -P {$enable_swow} --show-diff --show-slow 1000 --set-timeout 30 --color";
$options = implode(' ', array_map('trim', explode(' ', $options)));
$user_args = $argv;
array_shift($user_args);
$user_args = array_map('trim', $user_args);
$user_args = array_map('escapeshellarg', $user_args);
$user_args = escapeshellarg(implode(' ', $user_args));

notice('Start testing Swow');
exit(passthru(sprintf('%s "%s %s "%s', $shell, $cmd, $options, $user_args)));
