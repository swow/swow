--TEST--
swow_socket: IPC handle
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

$pipePath = getRandomPipePath();
$mainSocket = new Socket(Socket::TYPE_IPCC);
$workerSocket = new Socket(Socket::TYPE_PIPE);
$workerSocket->bind($pipePath)->listen();
$wr = new WaitReference();
Coroutine::run(function () use ($mainSocket, $pipePath, $wr) {
    $mainSocket->connect($pipePath);
});
$workerChannel = new Socket(Socket::TYPE_IPCC);
$workerSocket->acceptTo($workerChannel);
$wr::wait($wr);

// prepare server
$wr = new WaitReference();
$tcpServer = new Socket(Socket::TYPE_TCP);
$tcpServer->bind('127.0.0.1')->listen();
Coroutine::run(function () use ($tcpServer, $wr) {
    $connection = $tcpServer->accept();
    echo $connection->recvString() . PHP_LF;
    $connection->sendString('Hello Client');
});
// prepare handle
$mainClient = new Socket(Socket::TYPE_TCP);
$mainClient->connect('127.0.0.1', $tcpServer->getSockPort());
// transfer handle
$mainSocket->sendHandle($mainClient);
$workerClient = new Socket(Socket::TYPE_TCP);
$workerChannel->acceptTo($workerClient);
// testing on received handle
echo $workerClient->sendString('Hello Server')->recvString() . PHP_LF;
$wr::wait($wr);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Hello Server
Hello Client
Done
