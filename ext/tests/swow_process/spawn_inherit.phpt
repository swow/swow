--TEST--
swow_process: ChildProcess inherit stdio and exit status
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

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);']);

Assert::null($process->getStdin());
Assert::null($process->getStdout());
Assert::null($process->getStderr());

$status = $process->wait();
Assert::true($status->isExited());
Assert::false($status->isSignaled());
Assert::false($status->isStopped());
Assert::same($status->getExitCode(), 0);
echo "Done\n";

?>
--EXPECT--
Done
