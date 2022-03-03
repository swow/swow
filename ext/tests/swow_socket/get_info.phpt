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
use Swow\SocketException;

class StrangeSocket extends Socket
{
    public function __construct(int $type = self::TYPE_TCP)
    {
        // if not constructed, socket is not available
        Assert::false($this->isAvailable());
        parent::__construct($type);
    }
}

$socket = new StrangeSocket(StrangeSocket::TYPE_TCP);

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
$recvBufferSize = $socket->getRecvBufferSize();
$recvBufferSize += Buffer::PAGE_SIZE / 2;
$socket->setRecvBufferSize($recvBufferSize);
Assert::greaterThanEq($socket->getRecvBufferSize(), $recvBufferSize);

$socket->setSendBufferSize(0);
$sendBufferSize = $socket->getSendBufferSize();
$sendBufferSize += Buffer::PAGE_SIZE / 2;
$socket->setsendBufferSize($sendBufferSize);
Assert::greaterThanEq($socket->getSendBufferSize(), $sendBufferSize);

Assert::true($socket->isAvailable());
Assert::true($socket->isOpen());
Assert::true($socket->isServer());
Assert::false($socket->isEstablished());
Assert::greaterThanEq($socket->getFd(), 0);
Assert::string($socket->getSockAddress());
Assert::greaterThan($socket->getSockPort(), 0);

$noticer = new Channel(0);
Coroutine::run(function () use ($noticer, $socket) {
    $client = new Socket(Socket::TYPE_TCP);
    Assert::false($client->isOpen());
    $client->connect($socket->getSockAddress(), $socket->getSockPort());
    Assert::true($client->isOpen());
    Assert::true($client->isClient());
    $noticer->push(1);
    $client->close();
});

$connection = $socket->accept();
//Assert::same($connection->getConnectionError(), 0);

try {
    Assert::true($connection->isOpen());
    Assert::true($connection->isEstablished());
    Assert::true($connection->isServerConnection());
    Assert::string($connection->getPeerAddress());
    Assert::greaterThan($connection->getPeerPort(), 0);
    $connection->checkLiveness();
} finally {
    // tell client to shut down
    $noticer->pop();
}

try {
    Assert::same($connection->recvString(), '');
    while (true) {
        $connection->checkLiveness();
        msleep(1);
    }
} catch (SocketException $e) {
    Assert::same($e->getCode(), $connection->getConnectionError());
    Assert::same($e->getCode(), Errno::ECONNRESET);
}

$connection->close();
Assert::false($connection->isEstablished());

$socket->close();

$socket = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($socket) {
    $socket->setConnectTimeout(1000);
    // a class E reserved ip that cannot be reached
    try {
        $socket->connect('244.0.0.1', 1234);
        echo "Connect should not success\n";
    } catch (SocketException $e) {
        Assert::same($e->getCode(), Errno::ECANCELED);
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
