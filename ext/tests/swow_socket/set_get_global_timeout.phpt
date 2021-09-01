--TEST--
swow_socket: setXXGlobalTimeout getXXGlobalTimeout
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;

Socket::setGlobalTimeout(1);
Assert::same(Socket::getGlobalDnsTimeout(), 1);
Assert::same(Socket::getGlobalConnectTimeout(), 1);
Assert::same(Socket::getGlobalHandshakeTimeout(), 1);
Assert::same(Socket::getGlobalReadTimeout(), 1);
Assert::same(Socket::getGlobalWriteTimeout(), 1);

Socket::setGlobalAcceptTimeout(1);
Assert::same(Socket::getGlobalAcceptTimeout(), 1);

$socket = new Socket(Socket::TYPE_TCP);
Assert::same($socket->getDnsTimeout(), 1);
Assert::same($socket->getConnectTimeout(), 1);
Assert::same($socket->getHandshakeTimeout(), 1);
Assert::same($socket->getReadTimeout(), 1);
Assert::same($socket->getWriteTimeout(), 1);
Assert::same($socket->getAcceptTimeout(), 1);

$socket->close();

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
