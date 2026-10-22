--TEST--
swow_stream: manual TLS handshake retains the connection timeout and blocking state
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!in_array('tls', stream_get_transports(), true), 'TLS transport is required');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$server = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
Assert::true(is_resource($server));
$connection = null;
$acceptor = Coroutine::run(static function () use ($server, &$connection): void {
    $connection = stream_socket_accept($server);
    Assert::true(is_resource($connection));
});
$context = stream_context_create(['ssl' => ['verify_peer' => false, 'verify_peer_name' => false]]);
$client = stream_socket_client('tcp://' . stream_socket_get_name($server, false), $errno, $errstr, 0.1, STREAM_CLIENT_CONNECT, $context);
Assert::true(is_resource($client));
Assert::true(stream_get_meta_data($client)['blocked']);
$warning = false;
set_error_handler(static function (int $level, string $message) use (&$warning, $client): bool {
    Assert::same($level, E_WARNING);
    Assert::true(str_contains(strtolower($message), 'timed out'));
    Assert::true(stream_get_meta_data($client)['blocked']);
    $warning = true;
    fclose($client);
    return true;
});
$started = hrtime(true);
try {
    Assert::false(stream_socket_enable_crypto($client, true, STREAM_CRYPTO_METHOD_TLS_CLIENT));
} finally {
    restore_error_handler();
    if (is_resource($client)) {
        fclose($client);
    }
    if (is_resource($connection)) {
        fclose($connection);
    }
    fclose($server);
}
Assert::true($warning);
Assert::false(is_resource($client));
Assert::lessThan((hrtime(true) - $started) / 1e9, 3.0);
echo "Done\n";
?>
--EXPECT--
Done
