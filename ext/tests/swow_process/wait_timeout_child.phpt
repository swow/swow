--TEST--
swow_process: ChildProcess wait timeout
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

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);']);

try {
    $process->wait(10);
    echo "No timeout\n";
} catch (ProcessException $exception) {
    echo "Timeout\n";
}

$process->kill(\Swow\Process\SIGKILL);
$process->wait();
echo "Done\n";

?>
--EXPECT--
Timeout
Done
