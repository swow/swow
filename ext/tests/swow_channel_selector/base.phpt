--TEST--
swow_selector: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;
use Swow\Selector;

$channel1 = new Channel();
$channel2 = new Channel();
$channel3 = new Channel();

Coroutine::run(static function () use ($channel1): void {
    usleep(1000);
    $channel1->push('one');
});
Coroutine::run(static function () use ($channel2): void {
    usleep(2000);
    $channel2->push('two');
});
Coroutine::run(static function () use ($channel3): void {
    usleep(3000);
    $channel3->push('three');
});

$s = new Selector();
for ($n = 3; $n--;) {
    $channel = $s
        ->pop($channel1)
        ->pop($channel2)
        ->pop($channel3)
        ->commit();
    $data = $s->fetch();
    if ($channel === $channel1) {
        Assert::same($data, 'one');
    } elseif ($channel === $channel2) {
        Assert::same($data, 'two');
    } elseif ($channel === $channel3) {
        Assert::same($data, 'three');
    } else {
        Assert::false('impossible');
    }
    Assert::same($s->getLastEvent(), Selector::EVENT_POP);
    echo $data . "\n";
}

?>
--EXPECT--
one
two
three
