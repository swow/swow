--TEST--
swow_stream: persistent stream is closed before the event loop is destroyed
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    die('skip TSan cannot handle PHP bailout');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
$server = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
if ($server === false) {
    exit("Failed to create server: {$errstr}\n");
}
$name = stream_socket_get_name($server, false);
$client = stream_socket_client("tcp://{$name}", $errno, $errstr, 5,
    STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
if ($client === false) {
    exit("Failed to create persistent client: {$errstr}\n");
}
echo "done\n";
?>
--EXPECT--
done
