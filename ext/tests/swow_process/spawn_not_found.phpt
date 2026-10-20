--TEST--
swow_process: ChildProcess spawn failure
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessException;

try {
    ChildProcess::spawn(['/nonexistent/swow-child']);
    echo "No exception\n";
} catch (ProcessException $exception) {
    echo "Exception\n";
}

?>
--EXPECT--
Exception
