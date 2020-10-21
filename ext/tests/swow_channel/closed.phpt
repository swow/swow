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
use const Swow\Errno\ECLOSED;

$channel = new Channel();
$channel->close();
Assert::false($channel->isAvailable());
try {
    $channel->pop();
} catch (Channel\Exception $exception) {
    Assert::same($exception->getCode(), ECLOSED);
}
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
