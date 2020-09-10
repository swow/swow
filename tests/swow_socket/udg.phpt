--TEST--
swow_socket: udg
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;
use const Swow\Errno\ECANCELED;

if (stripos(PHP_OS, 'Linux') !== false) {
    /* linux abstract name */
    if (mt_rand(0, 1)) {
        define('SERVER_SOCK', '');
        define('CLIENT_SOCK', '');
    } else {
        define('SERVER_SOCK', "\0swow_server_" . getRandomBytes(8));
        define('CLIENT_SOCK', "\0swow_client_" . getRandomBytes(8));
    }
} else {
    define('SERVER_SOCK', '/tmp/swow_server_' . getRandomBytes(8) . '.sock');
    define('CLIENT_SOCK', '/tmp/swow_client_' . getRandomBytes(8) . '.sock');
}

foreach ([false, true] as $useConnect) {
    $server = new Socket(Socket::TYPE_UDG);
    Coroutine::run(function () use ($server) {
        $server->bind(SERVER_SOCK);
        try {
            while (true) {
                $message = $server->recvStringFrom(TEST_MAX_LENGTH, $address);
                if (!$message) {
                    break;
                }
                $server->sendStringTo($message, $address);
            }
        } catch (Socket\Exception $exception) {
            Assert::same($exception->getCode(), ECANCELED);
        }
    });

    $wr = new WaitReference();
    Coroutine::run(function () use ($server, $useConnect, $wr) {
        $client = new Socket(Socket::TYPE_UDG);
        $client->bind(CLIENT_SOCK);
        if ($useConnect) {
            $client->connect($server->getSockAddress());
        }
        $randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH);
        for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
            if ($useConnect) {
                $client->sendString($randoms[$n]);
            } else {
                $client->sendStringTo($randoms[$n], $server->getSockAddress());
            }
        }
        for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
            if ($useConnect) {
                $packet = $client->recvString(TEST_MAX_LENGTH);
            } else {
                $packet = $client->recvStringFrom(TEST_MAX_LENGTH, $address);
                Assert::same($address, $server->getSockAddress());
            }
            Assert::same($packet, $randoms[$n]);
        }
        $client->close();
    });
    WaitReference::wait($wr);
    $server->close();
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
