--TEST--
swow_stream: exported socket streams retain identity, protocol and descriptor ownership
--EXTENSIONS--
sockets
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$listener = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
$client = $accepted = $stream = null;
try {
    Assert::true(socket_bind($listener, '127.0.0.1', 0));
    Assert::true(socket_listen($listener));
    Assert::true(socket_getsockname($listener, $ip, $port));
    $client = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    Assert::true(socket_connect($client, $ip, $port));
    $accepted = socket_accept($listener);
    Assert::true(socket_set_nonblock($accepted));
    $stream = socket_export_stream($accepted);
    Assert::same(socket_export_stream($accepted), $stream);
    Assert::same(stream_get_meta_data($stream)['stream_type'], 'tcp_socket');
    Assert::false(stream_get_meta_data($stream)['blocked']);
    Assert::true(stream_set_blocking($stream, true));
    Assert::true(stream_set_timeout($stream, 1));
    Assert::same(socket_write($client, "body\0data"), 9);
    Assert::same(fread($stream, 9), "body\0data");
    Assert::same(fwrite($stream, 'reply'), 5);
    Assert::same(socket_read($client, 5), 'reply');
    $weak = WeakReference::create($accepted);
    unset($accepted);
    Assert::null($weak->get());
    // Dropping the Socket wrapper must not close a stream still held by its caller.
    Assert::same(fwrite($stream, 'alive'), 5);
    Assert::same(socket_read($client, 5), 'alive');
    $imported = socket_import_stream($stream);
    Assert::same(socket_export_stream($imported), $stream);
    unset($imported);
} finally {
    unset($accepted);
    if (is_resource($stream)) {
        fclose($stream);
    }
    foreach ([$client, $listener] as $socket) {
        if ($socket !== null) {
            socket_close($socket);
        }
    }
}

$socket = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
$sender = $stream = null;
try {
    Assert::true(socket_bind($socket, '127.0.0.1', 0));
    Assert::true(socket_getsockname($socket, $ip, $port));
    $stream = socket_export_stream($socket);
    Assert::same(stream_get_meta_data($stream)['stream_type'], 'udp_socket');
    Assert::same(socket_export_stream($socket), $stream);
    $sender = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    Assert::same(socket_sendto($sender, 'packet', 6, 0, $ip, $port), 6);
    Assert::same(stream_socket_recvfrom($stream, 64, 0, $peer), 'packet');
    Assert::same(stream_socket_sendto($stream, 'response', 0, $peer), 8);
    Assert::same(socket_recvfrom($sender, $reply, 64, 0, $replyIp, $replyPort), 8);
    Assert::same($reply, 'response');
    Assert::same($replyIp, $ip);
    Assert::same($replyPort, $port);
} finally {
    socket_close($socket);
    if ($sender !== null) {
        socket_close($sender);
    }
}
Assert::throws(static function () use ($socket): void {
    socket_export_stream($socket);
}, Error::class, expectMessage: '/already been closed/');
echo "Done\n";
?>
--EXPECT--
Done
