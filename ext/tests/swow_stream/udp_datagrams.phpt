--TEST--
swow_stream: UDP peer addresses, peeking, replies and truncated datagram boundaries
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$server = stream_socket_server('udp://127.0.0.1:0', flags: STREAM_SERVER_BIND);
$client = null;
$writer = null;
try {
    $address = stream_socket_get_name($server, false);
    $context = stream_context_create(['socket' => ['bindto' => '127.0.0.1:0', 'so_broadcast' => true]]);
    $client = stream_socket_client('udp://' . $address, context: $context);
    Assert::true(stream_set_timeout($server, 1));
    Assert::true(stream_set_timeout($client, 1));
    Assert::same(stream_socket_get_name($client, true), $address);
    Assert::false(stream_get_meta_data($client)['seekable']);
    $body = "first\0packet";
    $writer = Coroutine::run(static function () use ($client, $body): void {
        usleep(10000);
        Assert::same(fwrite($client, $body), strlen($body));
    });
    $peer = null;
    Assert::same(stream_socket_recvfrom($server, strlen($body), STREAM_PEEK, $peer), $body);
    Assert::same($peer, stream_socket_get_name($client, false));
    Assert::true(stream_set_blocking($server, false));
    Assert::false(stream_get_meta_data($server)['blocked']);
    Assert::same(stream_socket_recvfrom($server, strlen($body), STREAM_PEEK), $body);
    Assert::true(stream_set_blocking($server, true));
    Assert::same(stream_socket_recvfrom($server, strlen($body), 0, $peer), $body);
    Assert::same(stream_socket_sendto($server, 'reply', 0, $peer), 5);
    Assert::same(stream_socket_recvfrom($client, 5, 0, $replyPeer), 'reply');
    Assert::same($replyPeer, $address);

    Assert::same(stream_socket_sendto($client, 'long datagram'), 13);
    Assert::same(stream_socket_sendto($client, 'next'), 4);
    // A short receive discards the rest of that datagram, preserving the following boundary.
    Assert::same(stream_socket_recvfrom($server, 3), 'lon');
    Assert::same(stream_socket_recvfrom($server, 16), 'next');
    Assert::false(stream_get_meta_data($server)['timed_out']);
} finally {
    if ($writer?->isAlive()) {
        $writer->kill();
    }
    foreach ([$client, $server] as $stream) {
        if (is_resource($stream)) {
            fclose($stream);
        }
    }
}
echo "Done\n";
?>
--EXPECT--
Done
