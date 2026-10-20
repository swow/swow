--TEST--
swow_process: ChildProcess spawn and wait
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(42);']);
$status = $process->wait();

Assert::same($status->getExitCode(), 42);
Assert::true($process->hasExited());
echo "Done\n";

?>
--EXPECT--
Done
