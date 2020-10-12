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

require_once __DIR__ . '/../tools.php';

use function Swow\Tools\br;
use function Swow\Tools\error;
use function Swow\Tools\notice;
use function Swow\Tools\log;
use function Swow\Tools\ok;
use function Swow\Tools\warn;

if (PHP_OS_FAMILY === 'Windows') {
    error('Not supported on Windows temporarily');
}

$workSpace = realpath(__DIR__ . '/../../');
$swowSo = "{$workSpace}/.libs/swow.so";

$commandArray = ["cd {$workSpace}"];
// TODO: option --rebuild
if (!file_exists("{$workSpace}/configure")) {
    $commandArray[] = 'phpize';
}
if (!file_exists("{$workSpace}/config.log")) {
    $commandArray[] = './configure --enable-debug';
}
// TODO: use Swow\Cpu module
$cpuCount = (int) shell_exec(__DIR__ . '/../../deps/libcat/tools/cpu_count.sh');
// TODO: option --silent
// TODO: CFLAGS/CXXFLAGS Werror
$commandArray[] = "make -j{$cpuCount} > /dev/null";
$command = implode(" && \\\n", $commandArray);

passthru($command, $status);
if ($status !== 0) {
    error('Make failed');
}
ok('Make complete');
br();

$checkCommand = "/usr/bin/env php -n -d extension={$swowSo} --ri swow";
ob_start();
passthru($checkCommand, $status);
$swowInfo = ob_get_clean();
if ($status === 0) {
    log("> {$checkCommand}");
    log($swowInfo);
} else {
    warn("Get extension info failed, you can run `{$checkCommand}` to confirm it");
}

notice('Install the extension to your system requires root privileges');
passthru('sudo make install');

br();
ok('Do\'not forget to add \'extension=swow\' to your PHP ini');
