--TEST--
swow_socket: udp
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

Socket::setGlobalTimeout(1000);

$wr = new WaitReference();
$randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH_LOW);

$server = new Socket(Socket::TYPE_UDP);
$client = new Socket(Socket::TYPE_UDP);

// a simple udp echo server
Coroutine::run(function () use ($server, $client, $randoms, $wr) {
    $server->bind('127.0.0.1');
    foreach ($randoms as $_) {
        $packet = $server->recvStringFrom(TEST_MAX_LENGTH_LOW + 4, $ip, $port);
        if (!$packet) {
            break;
        }
        $index = unpack('Nindex', substr($packet, 0, 4))['index'];
        Assert::keyExists($randoms, $index);
        $random = substr($packet, 4);
        Assert::same($random, $randoms[$index]);
        $server->sendStringTo($packet, $ip, $port);
    }
});

// use coroutine to avoid buffer exhaust and packet drop
Coroutine::run(function () use ($server, $client, $randoms, $wr) {
    foreach ($randoms as $n => $random) {
        $index_bin = pack('N', $n);
        $client->sendStringTo($index_bin . $random, $server->getSockAddress(), $server->getSockPort());
    }
});

foreach ($randoms as $_) {
    $packet = $client->recvStringFrom(TEST_MAX_LENGTH_LOW + 4, $ip, $port);
    $index = unpack('Nindex', substr($packet, 0, 4))['index'];
    Assert::keyExists($randoms, $index);
    $random = substr($packet, 4);
    Assert::same($random, $randoms[$index]);
}

WaitReference::wait($wr);
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
