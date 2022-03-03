--TEST--
swow_socket: exception
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

$wr = new WaitReference();

$random = getRandomBytes(TEST_MAX_LENGTH_LOW);

$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($server, $random, $wr) {
    $server->bind('127.0.0.1')->listen();
    try {
        $connection = $server->accept();
        Coroutine::run(function () use ($connection, $random, $wr) {
            try {
                $connection->readString(TEST_MAX_LENGTH_LOW + 1);
                Assert::assert(0 && 'never here');
            } catch (SocketException $exception) {
                Assert::same($exception->getCode(), Errno::ECONNRESET);
                Assert::same($exception->getReturnValue(), $random);
            }
        });
    } catch (SocketException $exception) {
        Assert::same($exception->getCode(), Errno::ECANCELED);
    }
});

Coroutine::run(function () use ($server, $random, $wr) {
    $client = new Socket(Socket::TYPE_TCP);
    $client->connect($server->getSockAddress(), $server->getSockPort());
    $client->sendString($random);
    $client->close();
    $server->close();
});

WaitReference::wait($wr);

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
