<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twose@qq.com>
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

if (PHP_OS_FAMILY === 'Windows') {
    error('Not supported on Windows temporarily');
}

$workSpace = realpath(__DIR__ . '/../../');
$swowSo = "{$workSpace}/.libs/swow.so";

// TODO: use Swow\Cpu module
if (PHP_OS_FAMILY === 'Darwin') {
    $cpuCount = (int) `echo $(sysctl -n machdep.cpu.core_count)`;
} elseif (PHP_OS_FAMILY === 'Linux') {
    $cpuCount = (int) `echo $(/usr/bin/nproc)`;
}
if (($cpuCount ?? 0) <= 0) {
    $cpuCount = 1;
}

$commandArray = ["cd {$workSpace}"];
// TODO: option --rebuild
if (!file_exists("{$workSpace}/configure")) {
    $commandArray[] = 'phpize';
}
if (!file_exists("{$workSpace}/config.log")) {
    $commandArray[] = './configure --enable-debug';
}
// TODO: option --silent
$commandArray[] = "make -j{$cpuCount}";
$command = implode(" && \\\n", $commandArray);

passthru($command, $status);
if ($status !== 0) {
    error('Make failed');
}
ok('Make complete');
br();

$checkCommand = "/usr/bin/env php -n -d extension={$swowSo} --ri swow";
ob_start();
passthru($checkCommand);
$swowInfo = ob_get_clean();
if (trim($swowInfo)) {
    log("> {$checkCommand}");
    log($swowInfo);
} else {
    notice("You can run `{$checkCommand}` to check if Swow is available");
}

notice('Install the extension to your system requires root privileges');
passthru('sudo make install');

br();
ok('Do\'not forget to add \'extension=swow\' to your PHP ini');
