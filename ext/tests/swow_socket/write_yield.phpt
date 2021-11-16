--TEST--
swow_socket: write yield
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

$wrServer = new WaitReference();
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($server, $wrServer) {
    $server->bind('127.0.0.1')->listen();
    try {
        while (true) {
            $connection = $server->accept();
            Coroutine::run(function () use ($connection, $wrServer) {
                try {
                    while (true) {
                        $connection->sendString($connection->readString(TEST_MAX_LENGTH_HIGH));
                    }
                } catch (Socket\Exception $exception) {
                    Assert::same($exception->getCode(), ECONNRESET);
                }
            });
        }
    } catch (Socket\Exception $exception) {
        Assert::same($exception->getCode(), ECANCELED);
    }
});

$wrClient = new WaitReference();
$client = new Socket(Socket::TYPE_TCP);
$client->connect($server->getSockAddress(), $server->getSockPort());
$randoms = getRandomBytesArray(TEST_MAX_REQUESTS_LOW, TEST_MAX_LENGTH_HIGH);
Coroutine::run(function () use ($server, $client, $randoms, $wrClient) {
    for ($n = 0; $n < TEST_MAX_REQUESTS_LOW; $n++) {
        $packet = $client->readString(TEST_MAX_LENGTH_HIGH);
        Assert::same($packet, $randoms[$n]);
    }
});
for ($n = 0; $n < TEST_MAX_REQUESTS_LOW; $n++) {
    $client->sendString($randoms[$n]);
}
WaitReference::wait($wrClient);
$client->close();
$server->close();
WaitReference::wait($wrServer);

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
