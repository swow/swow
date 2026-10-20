--TEST--
swow_process: ChildProcess stdin pipe
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY === 'Windows', 'POSIX implementation only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([
    PHP_BINARY, '-n', '-r', '$s = fread(STDIN, 5); fwrite(STDOUT, $s);',
], [
    'stdin' => 'pipe',
    'stdout' => 'pipe',
]);

$stdin = $process->getStdin();
Assert::true(is_resource($stdin));
fwrite($stdin, 'hello');

$stdout = stream_get_contents($process->getStdout());
$process->wait();

Assert::same($stdout, 'hello');
echo "Done\n";

?>
--EXPECT--
Done
