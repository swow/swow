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
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

// TODO: test both file and abstract name
if (PHP_OS_FAMILY === 'Linux' && mt_rand(0, 1)) {
    /* linux abstract name */
    define('SERVER_SOCK', "\0swow_server_" . getRandomBytes(8));
    define('CLIENT_SOCK', "\0swow_client_" . getRandomBytes(8));
} else {
    define('SERVER_SOCK', '/tmp/swow_server_' . getRandomBytes(8) . '.sock');
    define('CLIENT_SOCK', '/tmp/swow_client_' . getRandomBytes(8) . '.sock');
}

foreach ([false, true] as $useConnect) {
    $wrs = new WaitReference();
    $server = new Socket(Socket::TYPE_UDG);
    Coroutine::run(function () use ($server, $wrs) {
        $server->bind(SERVER_SOCK);
        try {
            while (true) {
                $message = $server->recvStringFrom(TEST_MAX_LENGTH, $address);
                if (!$message) {
                    break;
                }
                $server->sendStringTo($message, $address);
            }
        } catch (SocketException $exception) {
            Assert::same($exception->getCode(), Errno::ECANCELED);
        }
    });

    $wr = new WaitReference();
    $client = new Socket(Socket::TYPE_UDG);
    $client->bind(CLIENT_SOCK);
    if ($useConnect) {
        $client->connect($server->getSockAddress());
    }
    $randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH);
    Coroutine::run(function () use ($server, $client, $randoms, $useConnect, $wr) {
        for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
            if ($useConnect) {
                $packet = $client->recvString(TEST_MAX_LENGTH);
            } else {
                $packet = $client->recvStringFrom(TEST_MAX_LENGTH, $address);
                Assert::same($address, $server->getSockAddress());
            }
            Assert::same($packet, $randoms[$n]);
        }
    });
    for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
        if ($useConnect) {
            $client->sendString($randoms[$n]);
        } else {
            $client->sendStringTo($randoms[$n], $server->getSockAddress());
        }
    }

    WaitReference::wait($wr); /* wait read done */
    $client->close();

    $server->close();
    WaitReference::wait($wrs); /* wait server closed */
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
