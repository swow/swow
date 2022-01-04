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

use function Swow\Util\br;
use function Swow\Util\error;
use function Swow\Util\httpDownload;
use function Swow\Util\notice;
use function Swow\Util\passthru2;
use function Swow\Util\warn;

require __DIR__ . '/autoload.php';

$workspace = dirname(__DIR__) . '/ext';

$runTestsPath = "{$workspace}/run-tests.php";
if (!file_exists($runTestsPath)) {
    try {
        httpDownload('https://raw.githubusercontent.com/php/php-src/master/run-tests.php', $runTestsPath);
    } catch (RuntimeException $exception) {
        error($exception->getMessage());
    }
}

if ('Windows' === PHP_OS_FAMILY) {
    // No need to set nolimit on Windows
    $cmd = PHP_BINARY;
} else {
    $NOLIMIT = 8192;
    $cmd = "{ ulimit -n {$NOLIMIT} || echo 'Cannot set nolimit to {$NOLIMIT}, some tests may fail.' ; } && NO_INTERACTION=1 " . PHP_BINARY;
}

if (!extension_loaded(Swow::class)) {
    $enableSwow = ['-d', 'extension=swow'];
    $check = ['-d', 'extension=swow', '--ri', 'swow'];
    $status = passthru2($cmd, $check);
    br();
    if ($status !== 0) {
        warn('Failed load swow extension, have you installed swow (i.e. run composer build-extension)?');
        exit($status);
    }
} else {
    $enableSwow = [];
}
if (PHP_OS_FAMILY === 'Windows') {
    // TODO: parallel testing has bugs on Windows
    $cpuCount = 1;
} else {
    // TODO: use Swow\Cpu module
    $cpuCount = (int) shell_exec("{$workspace}/deps/libcat/tools/cpu_count.sh");
    if ($cpuCount <= 0) {
        $cpuCount = 4;
    }
}
$options = [
    '-n', $runTestsPath,
    '-P', ...$enableSwow,
    '--show-diff',
    '--show-slow', '1000',
    '--set-timeout', '30',
    '--color',
    "-j{$cpuCount}",
];
$userArgs = array_slice($argv, 1);

notice('Start testing Swow');
exit(passthru2($cmd, [...$options, ...$userArgs]));
