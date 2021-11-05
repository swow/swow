--TEST--
swow_coroutine: getState getStateName isAlive isAvailable
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

class InitialCoroutine extends Coroutine
{
    public function __construct(callable $callable)
    {
        // when Coroutine::__construct is not called on coro,
        // it will have Coroutine::STATE_NONE as state
        Assert::same($this->getState(), Coroutine::STATE_NONE);
        Assert::same($this->getStateName(), 'none');
        parent::__construct($callable);
    }
}
$coro = new InitialCoroutine(function () {});

$coro = new Coroutine(function () {
    // when it's started, Coroutine::STATE_RUNNING as state
    $coro = Coroutine::getCurrent();
    try {
        Assert::same($coro->getState(), Coroutine::STATE_RUNNING);
        Assert::same($coro->getStateName(), 'running');
        Assert::true($coro->isAvailable());
        Assert::true($coro->isAlive());
    } finally {
        Coroutine::yield();
    }
});
// when Coroutine::__construct is called on coro, while it's not started
// it will have Coroutine::STATE_WAITING as state
Assert::same($coro->getState(), Coroutine::STATE_WAITING);
Assert::same($coro->getStateName(), 'waiting');
Assert::true($coro->isAvailable());

$coro->resume();
try {
    // when it's started, and yield out
    // it will have Coroutine::STATE_WAITING as state
    Assert::same($coro->getState(), Coroutine::STATE_WAITING);
    Assert::same($coro->getStateName(), 'waiting');
    Assert::true($coro->isAvailable());
    Assert::true($coro->isAlive());
} finally {
    $coro->resume();
}
// when it's end up normally
// it will have Coroutine::STATE_DEAD as state
Assert::same($coro->getState(), Coroutine::STATE_DEAD);
Assert::same($coro->getStateName(), 'dead');

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
