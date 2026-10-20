--TEST--
swow_process: ChildProcess close streams
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);'], [
    'stdout' => 'pipe',
]);

$process->close();
Assert::null($process->getStdout());
Assert::false($process->hasExited());

$process->kill(\Swow\Process\SIGKILL);
$process->wait();
echo "Done\n";

?>
--EXPECT--
Done
