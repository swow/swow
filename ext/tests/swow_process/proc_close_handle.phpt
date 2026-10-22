--TEST--
swow_process: proc_close closes the child handle once
--FILE--
<?php
$process = proc_open([PHP_BINARY, '-n', '-r', 'echo "ok";'], [
    1 => ['pipe', 'w'],
], $pipes);
$output = stream_get_contents($pipes[1]);
$exitCode = proc_close($process);

var_dump($output, $exitCode);
?>
--EXPECT--
string(2) "ok"
int(0)
