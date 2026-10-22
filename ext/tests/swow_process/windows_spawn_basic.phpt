--TEST--
swow_process: Windows ChildProcess spawn
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS_FAMILY !== 'Windows', 'Windows only');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

Assert::true(defined('Swow\Process\SIGINT'));
Assert::true(defined('Swow\Process\SIGBREAK'));
Assert::true(defined('Swow\Process\SIGKILL'));
Assert::false(defined('Swow\Process\SIGTERM'));

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'exit(0);']);
$status = $process->wait();

Assert::same($status->getExitCode(), 0);
echo "Done\n";

?>
--EXPECT--
Done
