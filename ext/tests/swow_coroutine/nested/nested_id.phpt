--TEST--
swow_coroutine/nested: nested id
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Sync\WaitReference;

define('TEST_COROUTINE_MAIN_ID', Swow\Coroutine::getMain()->getId());

$wr = new WaitReference();

Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID);
Swow\Coroutine::run(function () use ($wr) {
    Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 1);
    msleep(1);
    Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 1);
});
Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID);
Swow\Coroutine::run(function () use ($wr) {
    Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 2);
    Swow\Coroutine::run(function () use ($wr) {
        Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 3);
        Swow\Coroutine::run(function () use ($wr) {
            Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID + 4);
            Swow\Coroutine::run(function () use ($wr) {
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
Assert::same(Swow\Coroutine::getCurrent()->getId(), TEST_COROUTINE_MAIN_ID);
WaitReference::wait($wr);

?>
--EXPECT--
Done
