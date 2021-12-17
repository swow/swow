--TEST--
swow_socket: sendString
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$socket = new Swow\Socket(Swow\Socket::TYPE_TCP);
$host = 'www4.bing.com';
$request =
    "GET / HTTP/1.1\r\n" .
    "Host: {$host}\n" .
    "\rFOO";
$socket->connect($host, 80)
    ->sendString($request, $socket->getWriteTimeout(), 0, strlen($request) - 3)
    ->sendString("\n");
$result = $socket->recvString();
phpt_var_dump($result);
Assert::contains($result, 'HTTP/1.1');

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
