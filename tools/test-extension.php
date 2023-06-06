#!/usr/bin/env php
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

use function Swow\Utils\br;
use function Swow\Utils\error;
use function Swow\Utils\httpDownload;
use function Swow\Utils\notice;
use function Swow\Utils\passthru;
use function Swow\Utils\warn;

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
$runTestsXPath = "{$workspace}/run-tests-x.php";
if (!file_exists($runTestsXPath)) {
    file_put_contents(
        $runTestsXPath,
        str_replace(
            '$optionals = [\'Zend\', \'tests\', \'ext\', \'sapi\'];',
            '$optionals = [\'ext/tests\'];',
            file_get_contents($runTestsPath)
        )
    );
}

if ('Windows' === PHP_OS_FAMILY) {
    // No need to set nolimit on Windows
    $shell = 'CMD /C';
    $cmd = PHP_BINARY;
} else {
    $shell = 'sh -c';
    $NOLIMIT = 8192;
    $cmd = "{ ulimit -n {$NOLIMIT} || echo 'Cannot set nolimit to {$NOLIMIT}, some tests may fail.' ; } && NO_INTERACTION=1 " . PHP_BINARY;
}

if (!extension_loaded(Swow::class)) {
    $status = passthru("{$shell} \"{$cmd} -d extension=swow --ri swow\"");
    br();
    if ($status !== 0) {
        warn('Failed load Swow extension, have you installed Swow (i.e. run composer build-extension)?');
        exit($status);
    }
    $enableSwow = ['-d', 'extension=swow'];
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
    '-n', $runTestsXPath,
    '-P', ...$enableSwow,
    '--show-diff',
    '--show-slow', '1000',
    '--set-timeout', '30',
    '--color',
    "-j{$cpuCount}",
];
$options = implode(' ', $options);
$userArgs = array_slice($argv, 1);
$userArgs = array_map('trim', $userArgs);
$userArgs = array_map('escapeshellarg', $userArgs);
$userArgs = escapeshellarg(implode(' ', $userArgs));

notice('Start testing Swow');
/*
 * Workaround for GitHub actions strange failures on Windows release builds,
 * we haven't found the reason why the program suddenly exited with 0,
 * but after we set default_socket_timeout to 300, it works.
 * We have tried:
 * `php -d extension=swow -d default_socket_timeout=1 -r "passthru('sleep 999');"`
 * or without swow, but results show that Swow has no effect on this...
 */
ini_set('default_socket_timeout', '300');
exit(passthru("{$shell} \"{$cmd} {$options} \"{$userArgs}"));
