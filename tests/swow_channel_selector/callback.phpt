--TEST--
swow_channel_selector: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Channel\CallbackSelector as Selector;
use Swow\Coroutine;

$channel1 = new Channel();
$channel2 = new Channel();
$channel3 = new Channel();

Coroutine::run(function () use ($channel1) {
    usleep(1000);
    $channel1->push('one');
});
Coroutine::run(function () use ($channel2) {
    usleep(2000);
    $channel2->push('two');
});
Coroutine::run(function () use ($channel3) {
    usleep(3000);
    $channel3->push('three');
});

$s = new Selector();
for ($n = 3; $n--;) {
    $s->casePop($channel1, function ($data) {
        echo $data . PHP_LF;
        Assert::same($data, 'one');
    })->casePop($channel2, function ($data) {
        echo $data . PHP_LF;
        Assert::same($data, 'two');
    })->casePop($channel3, function ($data) {
        echo $data . PHP_LF;
        Assert::same($data, 'three');
    })->select();
}

?>
--EXPECT--
one
two
three
