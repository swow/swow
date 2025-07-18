--TEST--
swow_stream: stream_socket_server with tls context options
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$args = array_merge([test_php_path()], php_options_with_swow(), [
    __DIR__ . '/socket_server.inc',
]);
$serverProc = proc_open($args, [
    0 => STDIN,
    1 => ['pipe', 'w'],
    2 => STDERR,
], $pipes, null, null);

$args = array_merge([test_php_path()], php_options_with_swow(), [
    __DIR__ . '/socket_client.inc',
]);
$clientProc = proc_open($args, [
    0 => $pipes[1],
    1 => STDOUT,
    2 => STDERR,
], $pipes, null, null);

proc_close($serverProc);
proc_close($clientProc);

echo "Done\n";
?>
--CLEAN--
<?php
require __DIR__ . '/../include/bootstrap.php';

@rmtree(__DIR__ . '/socket_serverX509');
@rmtree(__DIR__ . '/socket_clientX509');
?>
--EXPECT--
server end
client end
Done
