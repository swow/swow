--TEST--
swow_event: uncleaned resources (static)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$server = stream_socket_server('tcp://127.0.0.1:0');
$name = stream_socket_get_name($server, false);

static $socket;
$socket = stream_socket_client("tcp://{$name}");

fclose($server);

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
