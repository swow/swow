--TEST--
swow_process: ChildProcess destructor abandons a running child
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

$process = ChildProcess::spawn([PHP_BINARY, '-n', '-r', 'sleep(5);']);
unset($process);

echo "Done\n";

?>
--EXPECT--
Done
