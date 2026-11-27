--TEST--
swow_stream: php://fd on a socket does not block the scheduler
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_function_not_exist('stream_socket_pair');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

/*
 * A server that inherits its listening or connected socket as a file descriptor
 * (socket activation, a supervisor passing a connection) opens it with
 * php://fd/N. Here the descriptor comes from a socket pair of this process.
 */
[$local, $peer] = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
$fd = Swow\fileno($local);
$inherited = fopen("php://fd/{$fd}", 'r+');
Assert::true(is_resource($inherited));
echo 'stream_type: ', stream_get_meta_data($inherited)['stream_type'], "\n";

Coroutine::run(static function () use ($peer): void {
    usleep(200 * 1000);
    fwrite($peer, "request\n");
});
$line = SchedulerProbe::assertYields(static fn() => fgets($inherited), 200, 'fgets() on php://fd');
echo 'read: ', $line;

fwrite($inherited, "response\n");
echo 'peer read: ', fgets($peer);

stream_set_timeout($inherited, 0, 200 * 1000);
$data = SchedulerProbe::assertYields(static fn() => fread($inherited, 1), 200, 'fread() on php://fd waiting for its timeout');
var_dump($data === '' || $data === false, stream_get_meta_data($inherited)['timed_out']);
?>
--EXPECT--
stream_type: tcp_socket
read: request
peer read: response
bool(true)
bool(true)
