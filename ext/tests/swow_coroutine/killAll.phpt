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
    echo 'This should be killed' . PHP_LF;
});

$coros['normal'] = Coroutine::run(function () {
    // mock busy coroutine
    for ($i = 0; $i < 10000; $i++) {
        msleep(1);
    }
    echo 'This should be killed' . PHP_LF;
});

$coros['yield'] = Coroutine::run(function () {
    Coroutine::yield();
    echo 'This should be killed' . PHP_LF;
});

$chan = new Channel(0);
$coros['pop'] = Coroutine::run(function () use ($chan) {
    $chan->pop();
    echo 'This should be killed' . PHP_LF;
});

Assert::count(Coroutine::getAll(), count($coros) + 1);

Coroutine::killAll();

Assert::count(Coroutine::getAll(), 1);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
