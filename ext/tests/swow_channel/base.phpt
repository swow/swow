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
Coroutine::run(static function () use ($channel): void {
    $channel->push('Hello Swow');
});
echo $channel->pop() . "\n";

?>
--EXPECT--
Hello Swow
