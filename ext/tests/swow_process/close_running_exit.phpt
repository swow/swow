--TEST--
swow_process: close running process then exit cleans the event loop
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);']);
$process->close();

echo "Done\n";

?>
--EXPECT--
Done
