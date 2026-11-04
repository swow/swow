--TEST--
swow_socket: read into a buffer that took over a string copies before writing
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

$wr = new WaitReference();

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1', 0)->listen();
$port = $server->getSockPort();

Coroutine::run(static function () use ($server, $wr): void {
    $connection = $server->accept();
    $connection->send('hello');
    $connection->close();
    $server->close();
});

$client = new Socket(Socket::TYPE_TCP);
$client->connect('127.0.0.1', $port);

// A literal string is interned, and Buffer::write() adopts it when it fills
// the buffer, so the storage is read-only and shared.
$buffer = new Buffer(4);
$buffer->write(0, 'litrl');
Assert::same($buffer->toString(), 'litrl');

// Reading must copy the buffer away from the interned literal first.
Assert::same($client->read($buffer), 5);
Assert::same($buffer->toString(), 'hello');

$client->close();
$wr::wait($wr);

echo "Done\n";

?>
--EXPECT--
Done
