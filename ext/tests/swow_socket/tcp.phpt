--TEST--
swow_socket: tcp
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_max_open_files_less_than(256);
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
Coroutine::run(function () use ($server, $wrServer) {
    $server->bind('127.0.0.1')->listen(TEST_MAX_CONCURRENCY * 2);
    try {
        while (true) {
            $connection = $server->accept();
            Coroutine::run(function () use ($connection, $wrServer) {
                try {
                    while (true) {
                        $connection->sendString($connection->readString(TEST_MAX_LENGTH));
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
$randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH);
for ($c = 0; $c < TEST_MAX_CONCURRENCY; $c++) {
    Coroutine::run(function () use ($server, $randoms, $wrClient) {
        /* @var $client Socket */
        for ($r = TEST_MAX_REQUESTS; $r--;) {
            try {
                $client = new Socket(Socket::TYPE_TCP);
                $client->connect($server->getSockAddress(), $server->getSockPort());
                break;
            } catch (SocketException $exception) {
                /* Connection limitation on Windows latest and MacOS */
                if ($exception->getCode() !== Errno::ECONNREFUSED &&
                    $exception->getCode() !== Errno::ECONNRESET ||
                    $r === 0) {
                    throw $exception;
                }
                usleep(1000);
            }
        }
        Coroutine::run(function () use ($client, $randoms, $wrClient) {
            for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
                $packet = $client->readString(TEST_MAX_LENGTH);
                Assert::same($packet, $randoms[$n]);
            }
        });
        for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
            $client->sendString($randoms[$n]);
        }
    });
}
WaitReference::wait($wrClient);
$server->close();
WaitReference::wait($wrServer);

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
