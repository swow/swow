--TEST--
swow_socket: open
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_max_open_files_less_than(256);
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

$serverStream = stream_socket_server('tcp://127.0.0.1:0');
$server = new Socket(Socket::TYPE_TCP);
$server->open($serverStream)->listen();
$serverStreamName = stream_socket_get_name($serverStream, false);
$serverName = sprintf('%s:%s', $server->getSockAddress(), $server->getSockPort());
Assert::same($serverStreamName, $serverName);

$wr = new WaitReference();
Coroutine::run(static function () use ($serverName, $wr): void {
    echo file_get_contents("http://{$serverName}"), "\n";
});

$connection = $server->accept();
$request = $connection->recvString(8192);
$connection->send("HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\nDone");
$connection->close();
$wr::wait($wr);

?>
--EXPECT--
Done
