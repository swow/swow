--TEST--
swow_process: Windows unsupported signal
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY !== 'Windows', 'Windows only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessException;

try {
    \Swow\Process\kill(getmypid(), 9999);
    echo "kill\n";
} catch (ProcessException) {
    echo "kill-ex\n";
}

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);']);
try {
    $process->kill(9999);
    echo "instance\n";
} catch (ProcessException) {
    echo "instance-ex\n";
}

$process->kill(\Swow\Process\SIGKILL);
$process->wait();
echo "Done\n";

?>
--EXPECT--
kill-ex
instance-ex
Done
