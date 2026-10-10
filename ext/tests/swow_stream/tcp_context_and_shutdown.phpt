--TEST--
swow_stream: TCP context binding, accepted peer names and half-close ownership
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$context = stream_context_create(['socket' => [
    'tcp_nodelay' => false,
    'so_reuseaddr' => true,
    'ipv6_v6only' => true,
    'backlog' => 8,
]]);
$server = stream_socket_server('tcp://127.0.0.1:0', context: $context);
$client = $connection = null;
try {
    $address = stream_socket_get_name($server, false);
    $clientContext = stream_context_create(['socket' => ['bindto' => '127.0.0.1:0', 'tcp_nodelay' => false]]);
    $client = stream_socket_client('tcp://' . $address, timeout: 1, context: $clientContext);
    $connection = stream_socket_accept($server, 1, $acceptedPeer);
    Assert::same($acceptedPeer, stream_socket_get_name($client, false));
    Assert::same(stream_socket_get_name($connection, false), $address);
    Assert::same(stream_socket_get_name($connection, true), $acceptedPeer);
    Assert::same(stream_socket_get_name($client, true), $address);
    Assert::true(stream_set_timeout($connection, 1));
    Assert::true(stream_set_timeout($client, 1));
    Assert::isArray(fstat($connection));
    Assert::same(fwrite($client, "body\0data"), 9);
    Assert::true(stream_socket_shutdown($client, STREAM_SHUT_WR));
    Assert::same(stream_socket_recvfrom($connection, 9), "body\0data");
    Assert::same(fread($connection, 1), '');
    Assert::true(feof($connection));
    Assert::true(stream_get_meta_data($connection)['eof']);
    // Shutting down one write direction still permits the peer's response to be read.
    Assert::same(fwrite($connection, 'reply'), 5);
    Assert::true(stream_socket_shutdown($connection, STREAM_SHUT_WR));
    Assert::same(fread($client, 5), 'reply');
    Assert::same(fread($client, 1), '');
    Assert::true(feof($client));
    Assert::false(stream_get_meta_data($client)['timed_out']);
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
