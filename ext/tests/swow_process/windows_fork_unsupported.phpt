--TEST--
swow_process: Windows fork is unsupported
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!class_exists(\Swow\Process\ChildProcess::class), 'Windows ChildProcess is not implemented yet');
skip_if(PHP_OS_FAMILY !== 'Windows', 'Windows only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessException;

try {
    ChildProcess::fork(static fn(): int => 0);
    echo "No exception\n";
} catch (ProcessException $exception) {
    echo "Exception\n";
}

?>
--EXPECT--
Exception
