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

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

$wrServer = new WaitReference();
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(static function () use ($server, $wrServer): void {
    $server->bind('127.0.0.1')->listen(TEST_MAX_CONCURRENCY * 2);
    try {
        while (true) {
            $connection = $server->accept();
            Coroutine::run(static function () use ($connection, $wrServer): void {
                try {
                    while (true) {
                        $connection->send($connection->readString(TEST_MAX_LENGTH));
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
    Coroutine::run(static function () use ($server, $randoms, $wrClient): void {
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
        if (!isset($client)) {
            throw new RuntimeException('Failed to connect');
        }
        Coroutine::run(static function () use ($client, $randoms, $wrClient): void {
            for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
                $packet = $client->readString(TEST_MAX_LENGTH);
                Assert::same($packet, $randoms[$n]);
            }
        });
        for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
            // test send empty string/buffer here
            mt_rand(0, 1) ? $client->send('') : $client->send(new Buffer(0));
            $client->send($randoms[$n]);
        }
    });
}
WaitReference::wait($wrClient);
$server->close();
WaitReference::wait($wrServer);

echo "Done\n";

?>
--EXPECT--
Done
