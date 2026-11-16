--TEST--
swow_socket: enableCrypto accepts null options
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

// Socket::enableCrypto(?array $options = null): passing null explicitly
// must behave the same as passing nothing, instead of throwing TypeError

$server = new Socket(Socket::TYPE_TCP);
$server->bind('localhost', 0);
$port = $server->getSockPort();

Coroutine::run(static function () use ($server): void {
    $server->listen();

    // server without certificate: handshake fails, but not with TypeError
    Assert::throws(static function () use ($server): void {
        $server->accept()->enableCrypto(null);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');
});

$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client): void {
    $client->enableCrypto(null);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

echo "Done\n";

?>
--EXPECT--
Done
