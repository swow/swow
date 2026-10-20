--TEST--
swow_process: ChildProcess cwd and env
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$cwd = realpath(sys_get_temp_dir());
$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r', 'fwrite(STDOUT, getcwd() . "|" . getenv("SWOW_CHILD_ENV"));',
], [
    'cwd' => $cwd,
    'env' => ['SWOW_CHILD_ENV' => 'yes'],
    'stdout' => 'pipe',
]);

$out = stream_get_contents($process->getStdout());
$process->wait();

Assert::same($out, $cwd . '|yes');
echo "Done\n";

?>
--EXPECT--
Done
