--TEST--
swow_process: namespace kill on an arbitrary pid
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use function Swow\Process\kill;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);']);
$pid = $process->getPid();

kill($pid, 0);
kill($pid, \Swow\Process\SIGKILL);

$status = $process->wait();
if (PHP_OS_FAMILY === 'Windows') {
    Assert::true($status->isExited());
} else {
    Assert::true($status->isSignaled());
    Assert::same($status->getTermSignal(), \Swow\Process\SIGKILL);
}
echo "Done\n";

?>
--EXPECT--
Done
