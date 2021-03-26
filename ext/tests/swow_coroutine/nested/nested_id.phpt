--TEST--
swow_coroutine/nested: nested id
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--XFAIL--
Solve hard code
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

// FIXME: use constant
Assert::same(Swow\Coroutine::getCurrent()->getId(), 0);
Assert::same(Swow\Coroutine::getCurrent()->getPrevious()->getId(), 0);
Swow\Coroutine::run(function () {
    Assert::same(Swow\Coroutine::getCurrent()->getId(), 2);
    msleep(1);
    Assert::same(Swow\Coroutine::getCurrent()->getId(), 2);
});
Assert::same(Swow\Coroutine::getCurrent()->getId(), 1);
Assert::same(Swow\Coroutine::getCurrent()->getPrevious()->getId(), 0);
Swow\Coroutine::run(function () {
    Assert::same(Swow\Coroutine::getCurrent()->getId(), 3);
    Swow\Coroutine::run(function () {
        Assert::same(Swow\Coroutine::getCurrent()->getId(), 4);
        Swow\Coroutine::run(function () {
            Assert::same(Swow\Coroutine::getCurrent()->getId(), 5);
            Swow\Coroutine::run(function () {
                Assert::same(Swow\Coroutine::getCurrent()->getId(), 6);
                msleep(1);
                Assert::same(Swow\Coroutine::getCurrent()->getId(), 6);
                echo 'Done' . PHP_LF;
            });
            Assert::same(Swow\Coroutine::getCurrent()->getId(), 5);
        });
        Assert::same(Swow\Coroutine::getCurrent()->getId(), 4);
    });
    Assert::same(Swow\Coroutine::getCurrent()->getId(), 3);
});
Assert::same(Swow\Coroutine::getCurrent()->getPrevious()->getId(), 0);
Assert::same(Swow\Coroutine::getCurrent()->getId(), 1);
?>
--EXPECT--
Done
