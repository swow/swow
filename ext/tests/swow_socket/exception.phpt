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
use Swow\Socket;
use Swow\Sync\WaitReference;
use const Swow\Errno\ECANCELED;
use const Swow\Errno\ECONNRESET;

$wr = new WaitReference();

$random = getRandomBytes(TEST_MAX_LENGTH_LOW);

$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($server, $random, $wr) {
    $server->bind('127.0.0.1')->listen();
    try {
        $client = $server->accept();
        Coroutine::run(function () use ($client, $random, $wr) {
            try {
                $client->readString(TEST_MAX_LENGTH_LOW + 1);
                Assert::assert(0 && 'never here');
            } catch (Socket\Exception $exception) {
                Assert::same($exception->getCode(), ECONNRESET);
                Assert::same($exception->getReturnValue(), $random);
            }
        });
    } catch (Socket\Exception $exception) {
        Assert::same($exception->getCode(), ECANCELED);
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
