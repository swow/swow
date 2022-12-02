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
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

$wrServer = new WaitReference();
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(static function () use ($server, $wrServer): void {
    $server->bind('127.0.0.1')->listen();
    try {
        while (true) {
            $connection = $server->accept();
            Coroutine::run(static function () use ($connection, $wrServer): void {
                try {
                    while (true) {
                        $connection->send($connection->readString(TEST_MAX_LENGTH_HIGH));
                    }
                } catch (SocketException $exception) {
                    Assert::same($exception->getCode(), Errno::ECONNRESET);
                }
            });
        }
    } catch (SocketException $exception) {
        Assert::same($exception->getCode(), Errno::ECANCELED);
    }
});

$wrClient = new WaitReference();
$client = new Socket(Socket::TYPE_TCP);
$client->connect($server->getSockAddress(), $server->getSockPort());
$randoms = getRandomBytesArray(TEST_MAX_REQUESTS_LOW, TEST_MAX_LENGTH_HIGH);
Coroutine::run(static function () use ($server, $client, $randoms, $wrClient): void {
    for ($n = 0; $n < TEST_MAX_REQUESTS_LOW; $n++) {
        $packet = $client->readString(TEST_MAX_LENGTH_HIGH);
        Assert::same($packet, $randoms[$n]);
    }
});
for ($n = 0; $n < TEST_MAX_REQUESTS_LOW; $n++) {
    $client->send($randoms[$n]);
}
WaitReference::wait($wrClient);
$client->close();
$server->close();
WaitReference::wait($wrServer);

echo "Done\n";

?>
--EXPECT--
Done
