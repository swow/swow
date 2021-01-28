--TEST--
swow_socket: pipe
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
use const Swow\Errno\ENFILE;
use const Swow\Errno\ENOENT;

if (stripos(PHP_OS, 'WIN') === false) {
    define('SERVER_SOCK', '/tmp/swow_server_' . getRandomBytes(8) . '.sock');
    define('CLIENT_SOCK', '/tmp/swow_client_' . getRandomBytes(8) . '.sock');
} else {
    define('SERVER_SOCK', '\\\\?\\pipe\\swow_server_' . getRandomBytes(8));
    define('CLIENT_SOCK', '\\\\?\\pipe\\swow_client_' . getRandomBytes(8));
}

$server = new Socket(Socket::TYPE_PIPE);
Coroutine::run(function () use ($server) {
    $server->bind(SERVER_SOCK)->listen();
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
for ($c = 0; $c < TEST_MAX_CONCURRENCY_LOW; $c++) {
    Coroutine::run(function () use ($server, $wr) {
        $client = new Socket(Socket::TYPE_PIPE);
        if (TEST_MAX_CONCURRENCY_LOW === 1) {
            $client->bind(CLIENT_SOCK);
        }
        for ($r = TEST_MAX_REQUESTS; $r--;) {
            try {
                $client->connect($server->getSockAddress());
                break;
            } catch (Socket\Exception $exception) {
                /* Connection limitation on Windows */
                if ($exception->getCode() !== ENOENT) {
                    throw $exception;
                }
                usleep(1000);
            }
        }
        $randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH);
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
