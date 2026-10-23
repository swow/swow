--TEST--
swow_process: ChildProcess wait and kill after exit
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    exit('skip TSan cannot handle PHP bailout');
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

?>
--EXPECT--
wait-ex
kill-ex
