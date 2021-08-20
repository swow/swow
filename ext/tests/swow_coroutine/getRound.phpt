--TEST--
swow_coroutine: getRound getCurrentRound
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$coro = Coroutine::getCurrent();
Assert::same(Coroutine::getCurrentRound(), $coro->getRound());

$remote_coro = new Coroutine(function () use ($coro) {
    $remote_coro = Coroutine::getCurrent();
    $current_round = Coroutine::getCurrentRound();
    try {
        Assert::same($current_round, $remote_coro->getRound());
        Assert::same($current_round, $coro->getRound() + 1);
    } finally {
        Coroutine::yield();
        Assert::greaterThan($remote_coro->getRound(), $current_round);
    }
});

$remote_coro->resume();
$remote_coro->resume();

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
