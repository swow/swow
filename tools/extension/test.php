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
use function Swow\Tools\error;
use function Swow\Tools\notice;
use function Swow\Tools\passthru;
use function Swow\Tools\warn;

require __DIR__ . '/../tools.php';

$workspace = dirname(__DIR__, 2) . '/ext';

$runTestsPath = "{$workspace}/run-tests.php";
if (!file_exists($runTestsPath)) {
    $requestArgs = ['filename' => 'https://raw.githubusercontent.com/php/php-src/master/run-tests.php'];
    if (getenv('http_proxy')) {
        $requestArgs['context'] = stream_context_create([
            'http' => [
                'proxy' => str_replace(['http://'], 'tcp://', getenv('http_proxy')),
                'request_fulluri' => true,
            ],
        ]);
    }
    $runTestsSource = file_get_contents(...$requestArgs);
    if (!$runTestsSource) {
        error('Unable to download run-tests.php');
    }
    file_put_contents($runTestsPath, $runTestsSource);
    if (!file_exists($runTestsPath)) {
        error('Unable to put run-tests.php');
    }
}

$NOLIMIT = 8192;

if ('Windows' === PHP_OS_FAMILY) {
    // No need to set nolimit on Windows
    $shell = 'CMD /C';
    $cmd = PHP_BINARY;
} else {
    $shell = 'sh -c';
    $cmd = "{ ulimit -n {$NOLIMIT} || echo 'Cannot set nolimit to {$NOLIMIT}, some tests may fail.' ; } && NO_INTERACTION=1 " . PHP_BINARY;
}

if (!extension_loaded(Swow::class)) {
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
if (PHP_OS_FAMILY === 'Windows') {
    // TODO: parallel testing has bugs on Windows
    $cpuCount = 1;
} else {
    // TODO: use Swow\Cpu module
    $cpuCount = (int) `{$workspace}/deps/libcat/tools/cpu_count.sh`;
    if ($cpuCount <= 0) {
        $cpuCount = 4;
    }
}
$options = "-n {$runTestsPath} -P {$enable_swow} --show-diff --show-slow 1000 --set-timeout 30 --color -j{$cpuCount}";
$options = implode(' ', array_map('trim', explode(' ', $options)));
$user_args = $argv;
array_shift($user_args);
$user_args = array_map('trim', $user_args);
$user_args = array_map('escapeshellarg', $user_args);
$user_args = escapeshellarg(implode(' ', $user_args));

notice('Start testing Swow');
exit(passthru(sprintf('%s "%s %s "%s', $shell, $cmd, $options, $user_args)));
