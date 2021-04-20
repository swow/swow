--TEST--
swow_coroutine/nested: nested id
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

define('TEST_COROUTINE_MAIN_ID', Swow\Coroutine::getMain()->getId());

Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID);
Assert::same(Swow\Coroutine::getCurrent()->getPrevious()->getId(), TEST_COROUTINE_MAIN_ID - 1);
Swow\Coroutine::run(function () {
    Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 1);
    msleep(1);
    Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 1);
});
Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID);
Assert::same(Swow\Coroutine::getCurrent()->getPrevious()->getId(), TEST_COROUTINE_MAIN_ID - 1);
Swow\Coroutine::run(function () {
    Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 2);
    Swow\Coroutine::run(function () {
        Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 3);
        Swow\Coroutine::run(function () {
            Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 4);
            Swow\Coroutine::run(function () {
                Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 5);
                msleep(1);
                Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 5);
                echo 'Done' . PHP_LF;
            });
            Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 4);
        });
        Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 3);
    });
    Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 2);
});
Assert::same(Swow\Coroutine::getCurrent()->getPrevious()->getId(), TEST_COROUTINE_MAIN_ID - 1);
Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID);
?>
--EXPECT--
Done
