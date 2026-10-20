--TEST--
swow_process: ChildProcess invalid arguments
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
    ChildProcess::spawn([]);
    echo "empty\n";
} catch (ProcessException) {
    echo "empty-ex\n";
}

try {
    ChildProcess::spawn([123]);
    echo "type\n";
} catch (TypeError) {
    echo "type-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY], ['stdout' => 'bad']);
    echo "stdio\n";
} catch (ProcessException) {
    echo "stdio-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY], ['cwd' => []]);
    echo "cwd\n";
} catch (ProcessException) {
    echo "cwd-ex\n";
}

try {
    ChildProcess::spawn([PHP_BINARY], ['env' => 'bad']);
    echo "env\n";
} catch (ProcessException) {
    echo "env-ex\n";
}

?>
--EXPECT--
empty-ex
type-ex
stdio-ex
cwd-ex
env-ex
