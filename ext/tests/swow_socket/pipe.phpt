--TEST--
swow_socket: pipe
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

if (PHP_OS_FAMILY !== 'Windows') {
    define('SERVER_SOCK', '/tmp/swow_server_' . getRandomBytes(8) . '.sock');
    define('CLIENT_SOCK', '/tmp/swow_client_' . getRandomBytes(8) . '.sock');
} else {
    define('SERVER_SOCK', '\\\\?\\pipe\\swow_server_' . getRandomBytes(8));
    define('CLIENT_SOCK', '\\\\?\\pipe\\swow_client_' . getRandomBytes(8));
}

$wrServer = new WaitReference();
$server = new Socket(Socket::TYPE_PIPE);
Coroutine::run(function () use ($server, $wrServer){
    $server->bind(SERVER_SOCK)->listen(TEST_MAX_CONCURRENCY * 2);
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
for ($c = 0; $c < TEST_MAX_CONCURRENCY_LOW; $c++) {
    Coroutine::run(function () use ($server, $wrClient) {
        /* @var $client Socket */
        for ($r = TEST_MAX_REQUESTS; $r--;) {
            try {
                $client = new Socket(Socket::TYPE_PIPE);
                if (TEST_MAX_CONCURRENCY_LOW === 1) {
                    $client->bind(CLIENT_SOCK);
                }
                $client->connect($server->getSockAddress());
                break;
            } catch (SocketException $exception) {
                /* Connection limitation on Windows latest and MacOS */
                if ($exception->getCode() !== Errno::ENOENT || $r === 0) {
                    throw $exception;
                }
                usleep(1000);
            }
        }
        $randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH);
        Coroutine::run(function () use ($client, $randoms, $wrClient) {
            for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
                $packet = $client->readString(TEST_MAX_LENGTH);
                Assert::same($packet, $randoms[$n]);
            }
            $client->close();
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
