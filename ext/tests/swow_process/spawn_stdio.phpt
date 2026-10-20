--TEST--
swow_process: ChildProcess stdout and stderr pipes
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'fwrite(STDOUT, "out"); fwrite(STDERR, "err");'], [
    'stdout' => 'pipe',
    'stderr' => 'pipe',
]);

$stdout = stream_get_contents($process->getStdout());
$stderr = stream_get_contents($process->getStderr());
$status = $process->wait();

Assert::same($stdout, 'out');
Assert::same($stderr, 'err');
Assert::same($status->getExitCode(), 0);
echo "Done\n";

?>
--EXPECT--
Done
