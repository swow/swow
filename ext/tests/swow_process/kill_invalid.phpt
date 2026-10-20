--TEST--
swow_process: namespace kill invalid pid
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ProcessException;

use function Swow\Process\kill;

try {
    kill(9999999, \Swow\Process\SIGTERM);
    echo "No exception\n";
} catch (ProcessException $exception) {
    echo "Exception\n";
}

?>
--EXPECT--
Exception
