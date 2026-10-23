--TEST--
swow_process: ChildProcess close after exit
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    die('skip TSan cannot handle PHP bailout');
}

require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessException;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);']);
$process->wait();

$process->close();
$process->close();

try {
    $process->getPid();
    echo "pid\n";
} catch (ProcessException) {
    echo "pid-ex\n";
}

try {
    $process->wait();
    echo "wait\n";
} catch (ProcessException) {
    echo "wait-ex\n";
}

try {
    $process->kill(\Swow\Process\SIGTERM);
    echo "kill\n";
} catch (ProcessException) {
    echo "kill-ex\n";
}

try {
    $process->hasExited();
    echo "exited\n";
} catch (ProcessException) {
    echo "exited-ex\n";
}

echo "Done\n";

?>
--EXPECT--
pid-ex
wait-ex
kill-ex
exited-ex
Done
