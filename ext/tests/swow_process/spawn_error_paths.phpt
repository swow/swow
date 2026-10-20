--TEST--
swow_process: ChildProcess error paths
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessException;

try {
    ChildProcess::spawn(['swow-no-such-command'], ['env' => ['PATH' => dirname(PHP_BINARY)]]);
    echo "path\n";
} catch (ProcessException) {
    echo "path-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY], ['env' => ['A' => 1]]);
    echo "env\n";
} catch (ProcessException) {
    echo "env-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY], ['stdout' => ['file', '/nonexistent/dir/swow', 'w']]);
    echo "file\n";
} catch (ProcessException) {
    echo "file-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY], ['stdout' => ['file']]);
    echo "shape\n";
} catch (ProcessException) {
    echo "shape-ex\n";
}

?>
--EXPECT--
path-ex
env-ex
file-ex
shape-ex
