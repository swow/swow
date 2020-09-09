--TEST--
swow_channel: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

$channel = new Channel();
Coroutine::run(function () use ($channel) {
    $channel->push('Hello Swow');
});
echo $channel->pop() . PHP_LF;

?>
--EXPECT--
Hello Swow
