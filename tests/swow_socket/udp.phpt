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

$server = new Socket(Socket::TYPE_UDP);
Coroutine::run(function () use ($server) {
    $server->bind('127.0.0.1', 0);
    while (true) {
        $packet = $server->recvStringFrom(TEST_MAX_LENGTH_LOW, $ip, $port);
        if (!$packet) {
            break;
        }
        $server->sendStringTo($packet, $ip, $port);
    }
});

$client = new Socket(Socket::TYPE_UDP);
$randoms = getRandomBytesArray(TEST_MAX_REQUESTS, TEST_MAX_LENGTH_LOW);
for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
    $client->sendStringTo($randoms[$n], $server->getSockAddress(), $server->getSockPort());
}
for ($n = 0; $n < TEST_MAX_REQUESTS; $n++) {
    $packet = $client->recvStringFrom(TEST_MAX_LENGTH_LOW, $ip, $port);
    Assert::same($packet, $randoms[$n]);
}
$client->sendStringTo('', $server->getSockAddress(), $server->getSockPort());

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
