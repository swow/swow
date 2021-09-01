--TEST--
swow_socket: informative methods
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Channel;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Socket;

class StrangeSocket extends Socket
{
    public function __construct(int $type = \Swow\Socket::TYPE_TCP)
    {
        // if not constructed, socket is not available
        Assert::false($this->isAvailable());
        parent::__construct($type);
    }
}

$socket = new StrangeSocket(Socket::TYPE_TCP);

// lazy init here, if socket is not listening or connected
// fd will not be accessible
Assert::lessThan($socket->getFd(), 0);
$socket->close();

$socket = new Socket(Socket::TYPE_TCP);

// lazy init here, if socket is not listening or connected, fd will not be accessible
//Assert::false($socket->isAvailable());
//Assert::lessThan($socket->getFd(), 0);

Assert::same($socket->getType(), Socket::TYPE_TCP);
Assert::same($socket->getTypeName(), 'TCP');

$socket->bind('localhost')->listen(1);

$socket->setRecvBufferSize(0);
$recv_bufsiz = $socket->getRecvBufferSize();
$recv_bufsiz += Buffer::PAGE_SIZE / 2;
$socket->setRecvBufferSize($recv_bufsiz);
Assert::greaterThanEq($socket->getRecvBufferSize(), $recv_bufsiz);

$socket->setSendBufferSize(0);
$send_bufsiz = $socket->getSendBufferSize();
$send_bufsiz += Buffer::PAGE_SIZE / 2;
$socket->setsendBufferSize($send_bufsiz);
Assert::greaterThanEq($socket->getSendBufferSize(), $send_bufsiz);

Assert::false($socket->isEstablished());
Assert::true($socket->isAvailable());
Assert::greaterThanEq($socket->getFd(), 0);
Assert::string($socket->getSockAddress());
Assert::greaterThan($socket->getSockPort(), 0);

$noticer = new Channel(0);
Coroutine::run(function () use ($noticer, $socket) {
    $client = new Socket(Socket::TYPE_TCP);
    $client->connect($socket->getSockAddress(), $socket->getSockPort());
    $noticer->push(1);
    $client->close();
});

$conn = $socket->accept();
//Assert::same($conn->getConnectionError(), 0);

try {
    Assert::true($conn->isEstablished());
    $conn->checkLiveness();
} finally {
    // tell client to shut down
    $noticer->pop();
}

try {
    Assert::same($conn->recvString(), '');
    while (true) {
        $conn->checkLiveness();
        msleep(1);
    }
} catch (Swow\Socket\Exception $e) {
    Assert::same($e->getCode(), $conn->getConnectionError());
    Assert::same($e->getCode(), Errno\ECONNRESET);
}

$conn->close();
Assert::false($conn->isEstablished());

$socket->close();

$socket = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($socket) {
    $socket->setConnectTimeout(1000);
    // a class E reserved ip that cannot be reached
    try {
        $socket->connect('244.0.0.1', 1234);
        echo "Connect should not success\n";
    } catch (Swow\Socket\Exception$e) {
        Assert::same($e->getCode(), Errno\ECANCELED);
    } finally {
        $socket->close();
    }
});

try {
    Assert::same($socket->getIoState(), Socket::IO_FLAG_CONNECT);
    Assert::same($socket->getIoStateName(), 'connect');
    Assert::same($socket->getIoStateNaming(), 'connecting');
} finally {
    $socket->close();
}

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
