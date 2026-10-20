--TEST--
swow_process: ProcessExitStatus negative values
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);']);
$status = $process->wait();
Assert::same($status->getExitCode(), 0);
Assert::same($status->getTermSignal(), -1);

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);']);
$process->kill(\Swow\Process\SIGKILL);
$status = $process->wait();
Assert::true($status->isSignaled());
Assert::same($status->getExitCode(), -1);
Assert::same($status->getTermSignal(), \Swow\Process\SIGKILL);
echo "Done\n";

?>
--EXPECT--
Done
