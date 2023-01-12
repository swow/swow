--TEST--
swow_socket: SSL error
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;
use Swow\SocketException;
use Swow\Sync\WaitReference;

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
$wr = new WaitReference();
Coroutine::run(static function () use ($server, $wr): void {
    $connection = $server->accept();
    try {
        $connection->enableCrypto();
    } catch (SocketException $exception) {
        echo $exception->getMessage() . "\n";
        $connection->close();
    }
});
$client = new Socket(Socket::TYPE_TCP);
$client->connect($server->getSockAddress(), $server->getSockPort());
$client->send(str_repeat("\0", 1024));
$wr::wait($wr);

echo "Done\n";

?>
--EXPECTF--
Socket enable crypto failed, reason: SSL_do_handshake() failed (SSL: %s)
Done
