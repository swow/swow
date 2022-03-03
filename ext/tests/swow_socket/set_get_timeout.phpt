--TEST--
swow_socket: setXXTimeout getXXTimeout
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;

$socket = new Socket(Socket::TYPE_TCP);
$socket->setTimeout(1);
// setTimeout is not setting accept timeout
//Assert::notSame($socket->getAcceptTimeout(), 1);
// others will be set
Assert::same($socket->getDnsTimeout(), 1);
Assert::same($socket->getConnectTimeout(), 1);
Assert::same($socket->getHandshakeTimeout(), 1);
Assert::same($socket->getReadTimeout(), 1);
Assert::same($socket->getWriteTimeout(), 1);

$socket->close();

try {
    $socket = new Socket(Socket::TYPE_TCP);
    // a class E reserved ip that cannot be reached
    $socket->setConnectTimeout(1);
    Assert::same($socket->getConnectTimeout(), 1);
    $socket->connect('244.0.0.1', 1234);
    echo "Connect should not success\n";
} catch (SocketException $e) {
    Assert::same($e->getCode(), Errno::ETIMEDOUT);
} finally {
    $socket->close();
}

try {
    $socket = new Socket(Socket::TYPE_TCP);
    $socket->setDnsTimeout(1);
    Assert::same($socket->getDnsTimeout(), 1);
    // a not exist domain, use random to avoid cache
    $socket->connect(getRandomBytes(12) . '.not-exist.donot.help', 1234);
    echo "Connect should not success\n";
} catch (SocketException $e) {
    Assert::oneOf($e->getCode(), [Errno::ETIMEDOUT, Errno::EAI_NONAME]);
} finally {
    $socket->close();
}

$noticer = new Channel(0);

// dummy server with delay
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($noticer, $server) {
    $server->bind('127.0.0.1')->listen();
    $conn = $server->accept();
    $conn->setRecvBufferSize(0);
    $noticer->pop();
    // never reads
    $conn->close();
    $server->close();
});

try {
    $socket = new Socket(Socket::TYPE_TCP);
    $socket->setWriteTimeout(1);
    Assert::same($socket->getWriteTimeout(), 1);
    $socket->connect($server->getSockAddress(), $server->getSockPort());
    $socket->setSendBufferSize(0);
    while (true) {
        // send a 16k string to overflow buffer
        $socket->sendString(str_repeat('Hello SwowSocket', 1024));
    }
} catch (SocketException $e) {
    Assert::same($e->getCode(), Errno::ETIMEDOUT);
} finally {
    $noticer->push(1);
}

// dummy server with delay
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($noticer, $server) {
    $server->bind('127.0.0.1')->listen();
    $conn = $server->accept();
    $noticer->pop();
    $conn->close();
    $server->close();
});

try {
    $socket = new Socket(Socket::TYPE_TCP);
    $socket->setReadTimeout(1);
    Assert::same($socket->getReadTimeout(), 1);
    $socket->connect($server->getSockAddress(), $server->getSockPort());
    $socket->readString();
    echo "Recv should not success\n";
} catch (SocketException $e) {
    Assert::same($e->getCode(), Errno::ETIMEDOUT);
} finally {
    $noticer->push(1);
    $socket->close();
}

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();

try {
    $server->setAcceptTimeout(1);
    Assert::same($server->getAcceptTimeout(), 1);
    $server->accept();
} catch (SocketException $e) {
    Assert::same($e->getCode(), Errno::ETIMEDOUT);
} finally {
    $server->close();
}

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
