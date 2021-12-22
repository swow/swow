--TEST--
swow_socket: tcp setTcpNodelay setTcpKeepAlive setTcpAcceptBalance
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;

$socket = new Socket(Socket::TYPE_TCP);
$socket->bind('127.0.0.1');

// these methods cannot be tested simply, here only assert return types

// set TCP_NODELAY option on tcp socket
Assert::same($socket->setTcpNodelay(true), $socket);

// make multi-process servers serves balance
Assert::same($socket->setTcpAcceptBalance(true), $socket);

// set SO_KEEPALIVE option on socket
Assert::same($socket->setTcpKeepAlive(true, 1000), $socket);

$socket->close();

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
