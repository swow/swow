--TEST--
swow_channel: closed
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\ChannelException;
use Swow\Errno;

$channel = new Channel();
$channel->close();
Assert::false($channel->isAvailable());
try {
    $channel->pop();
} catch (ChannelException $exception) {
    Assert::same($exception->getCode(), Errno::ECLOSED);
}
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
