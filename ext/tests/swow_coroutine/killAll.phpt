--TEST--
swow_coroutine: killAll
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

$coros = [];

$coros['stuck'] = Coroutine::run(function () {
    // mock long blocking
    sleep(10);
    echo "This should be killed\n";
});

$coros['normal'] = Coroutine::run(function () {
    // mock busy coroutine
    for ($i = 0; $i < 10000; $i++) {
        msleep(1);
    }
    echo "This should be killed\n";
});

$coros['yield'] = Coroutine::run(function () {
    Coroutine::yield();
    echo "This should be killed\n";
});

$chan = new Channel(0);
$coros['pop'] = Coroutine::run(function () use ($chan) {
    $chan->pop();
    echo "This should be killed\n";
});

Assert::count(Coroutine::getAll(), count($coros) + 1);

Coroutine::killAll();

Assert::count(Coroutine::getAll(), 1);

echo "Done\n";
?>
--EXPECT--
Done
