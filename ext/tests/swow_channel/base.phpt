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

$channel = new Channel;
$coroutine = Coroutine::run(function () use ($channel) {
    $channel->push('Hello swow');
});
echo $channel->pop() . PHP_LF;

?>
--EXPECT--
Hello swow
