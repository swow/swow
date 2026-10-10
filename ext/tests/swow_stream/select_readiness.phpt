--TEST--
swow_stream: select preserves keys, buffered reads and zero-timeout results
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$server = stream_socket_server('tcp://127.0.0.1:0');
$client = null;
$connection = null;
try {
    $client = stream_socket_client('tcp://' . stream_socket_get_name($server, false));
    $connection = stream_socket_accept($server, 1);
    foreach (['stream_select', 'stream_select_unlimited'] as $select) {
        Assert::same(fwrite($client, 'ready'), 5);
        // A writable peer can return from select before incoming bytes become readable.
        Assert::same(stream_poll_one($connection, STREAM_POLLIN, 1000), STREAM_POLLIN);
        $read = ['peer' => $client, 'connection' => $connection];
        $write = ['peer' => $client];
        $except = null;
        Assert::greaterThan($select($read, $write, $except, 1), 0);
        Assert::same($read, ['connection' => $connection]);
        Assert::same($write, ['peer' => $client]);
        Assert::same(stream_poll_one($connection, STREAM_POLLIN, 0), STREAM_POLLIN);
        Assert::same(fread($connection, 1), 'r');
        $read = ['buffered' => $connection];
        $write = null;
        Assert::same($select($read, $write, $except, 0), 1);
        Assert::same($read, ['buffered' => $connection]);
        Assert::same(stream_poll_one($connection, STREAM_POLLIN, 0), STREAM_POLLIN);
        Assert::same(fread($connection, 4), 'eady');
        $read = [$connection];
        Assert::same($select($read, $write, $except, 0), 0);
        Assert::same($read, []);
        Assert::same(stream_poll_one($connection, STREAM_POLLIN, 0), STREAM_POLLNONE);
        Assert::throws(static function () use ($connection, $select): void {
            $read = [$connection];
            $write = $except = null;
            $select($read, $write, $except, -1);
        }, ValueError::class);
        Assert::throws(static function () use ($connection, $select): void {
            $read = [$connection];
            $write = $except = null;
            $select($read, $write, $except, 0, -1);
        }, ValueError::class);
    }
} finally {
    foreach ([$connection, $client, $server] as $stream) {
        if (is_resource($stream)) {
            fclose($stream);
        }
    }
}

echo "Done\n";
?>
--EXPECT--
Done
