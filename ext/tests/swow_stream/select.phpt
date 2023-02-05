--TEST--
swow_stream: select
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$server = stream_socket_server('tcp://127.0.0.1:0');
$serverName = stream_socket_get_name($server, false);

$wr = new WaitReference();
Coroutine::run(static function () use ($serverName, $wr): void {
    echo file_get_contents("http://{$serverName}"), "\n";
});

$read = [$server];
$write = null;
$except = null;
$n = stream_select($read, $write, $except, 10);
Assert::same($n, 1);
Assert::oneOf($server, $read);
Assert::isEmpty($write);
Assert::isEmpty($except);
$connection = stream_socket_accept($server);
$request = fread($connection, 8192);
fwrite($connection, "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\nDone");
fclose($connection);

$wr::wait($wr);

?>
--EXPECT--
Done
