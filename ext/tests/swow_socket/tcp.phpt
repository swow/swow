--TEST--
swow_socket: tcp
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
use const Swow\Errno\ECONNREFUSED;
use const Swow\Errno\ECONNRESET;

$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($server) {
    $server->bind('127.0.0.1')->listen();
    try {
        while (true) {
            $client = $server->accept();
            Coroutine::run(function () use ($client) {
                try {
                    while (true) {
                        $client->sendString($client->readString(TEST_MAX_LENGTH));
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

$wr = new WaitReference();
$randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH);
for ($c = 0; $c < TEST_MAX_CONCURRENCY; $c++) {
    Coroutine::run(function () use ($server, $wr, $randoms) {
        $client = new Socket(Socket::TYPE_TCP);
        for ($r = TEST_MAX_REQUESTS; $r--;) {
            try {
                $client->connect($server->getSockAddress(), $server->getSockPort());
                break;
            } catch (Socket\Exception $exception) {
                /* Connection limitation on Windows latest */
                if ($exception->getCode() !== ECONNREFUSED) {
                    throw $exception;
                }
                usleep(1000);
            }
        }
        for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
            $client->sendString($randoms[$n]);
        }
        for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
            $packet = $client->readString(TEST_MAX_LENGTH);
            Assert::same($packet, $randoms[$n]);
        }
        $client->close();
    });
}
WaitReference::wait($wr);
$server->close();

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
