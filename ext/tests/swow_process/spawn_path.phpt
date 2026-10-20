--TEST--
swow_process: ChildProcess PATH lookup
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
    basename(PHP_BINARY), '-n', '-r', 'exit(42);',
], [
    'env' => ['PATH' => dirname(PHP_BINARY)],
]);

Assert::same($process->wait()->getExitCode(), 42);
echo "Done\n";

?>
--EXPECT--
Done
