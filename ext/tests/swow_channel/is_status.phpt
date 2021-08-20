--TEST--
swow_channel: isReadable isWritable isFull isEmpty isAvailable
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;

$chan = new Channel(0);

// if channel is no buffer, it's not writable/readable
Assert::false($chan->isWritable());
Assert::false($chan->isReadable());
// size 0 meaning both full and empty
Assert::true($chan->isEmpty());
Assert::true($chan->isFull());

Assert::true($chan->isAvailable());
$chan->close();
Assert::false($chan->isAvailable());

$chan = new Channel(2);

// empty channel is writable
Assert::true($chan->isWritable());
// empty channel is not readable
Assert::false($chan->isReadable());
Assert::true($chan->isEmpty());
Assert::false($chan->isFull());

$chan->push(1);
// 1/2 is neither empty nor full, writable and readable
Assert::true($chan->isWritable());
Assert::true($chan->isReadable());
Assert::false($chan->isEmpty());
Assert::false($chan->isFull());

$chan->push(2);
// 2/2 is full, so it's not writable but readable
Assert::false($chan->isWritable());
Assert::true($chan->isReadable());
Assert::false($chan->isEmpty());
Assert::true($chan->isFull());

// pop out first element
// channel is not empty, so there is no context switching
Assert::same($chan->pop(), 1);
// now there are 1/2 elements in channel
Assert::true($chan->isWritable());
Assert::true($chan->isReadable());
Assert::false($chan->isEmpty());
Assert::false($chan->isFull());

// pop out second element
// channel is not empty, so there is no context switching
Assert::same($chan->pop(), 2);
// now there is no element in channel
Assert::true($chan->isWritable());
Assert::false($chan->isReadable());
Assert::true($chan->isEmpty());
Assert::false($chan->isFull());

Assert::true($chan->isAvailable());
$chan->close();
Assert::false($chan->isAvailable());

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
