--TEST--
swow_socket: reading into a locked buffer does not copy it away from the reader
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

$server = (new Socket(Socket::TYPE_TCP))->bind('127.0.0.1', 0)->listen();
$port = $server->getSockPort();

Coroutine::run(static function () use ($server, $wr): void {
    $connection = $server->accept();
    // hold the reader suspended so it sits in read() with the buffer locked
    usleep(600000);
    $connection->send('WXYZ');
    usleep(100000);
    $connection->close();
    $server->close();
});

$client = (new Socket(Socket::TYPE_TCP))->connect('127.0.0.1', $port);

// owned storage, so a later copy makes it shared
$buffer = new Buffer(16);
$buffer->append('aaaa');

$done = false;
Coroutine::run(static function () use ($client, $buffer, &$done): void {
    $client->read($buffer, 4, 4);
    $done = true;
});

// wait for the reader to take the lock and suspend inside read()
for ($i = 0; $i < 500 && !$buffer->isLocked(); $i++) {
    usleep(10000);
}
Assert::true($buffer->isLocked());
Assert::false($done);

// toString() returns the storage itself, so the buffer no longer owns it alone
$copy = $buffer->toString();
Assert::same($copy, 'aaaa');

// A second reader must be rejected before it can touch the storage. If the
// copy happened before the lock check, the buffer would take a new storage and
// the suspended reader would keep writing into the old one.
Coroutine::run(static function () use ($port, $buffer): void {
    $other = (new Socket(Socket::TYPE_TCP))->connect('127.0.0.1', $port);
    try {
        $other->read($buffer, 0, 1);
        Assert::fail('reading a locked buffer should throw');
    } catch (Error $exception) {
        Assert::contains($exception->getMessage(), 'locked');
    }
    $other->close();
});

usleep(100000);

// drop the only reference to the old storage while the reader still uses it
unset($copy);

for ($i = 0; $i < 500 && !$done; $i++) {
    usleep(10000);
}
Assert::true($done);
Assert::same($buffer->toString(), 'aaaaWXYZ');

$client->close();
$wr::wait($wr);

echo "Done\n";

?>
--EXPECT--
Done
