--TEST--
swow_process: ChildProcess fork and wait
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'fork is POSIX only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::fork(static fn(): int => 42);
$status = $process->wait();

Assert::same($status->getExitCode(), 42);
Assert::true($process->hasExited());
echo "Done\n";

?>
--EXPECT--
Done
