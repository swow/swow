--TEST--
swow_coroutine/output: ob_* in coroutine
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

ob_start();
echo 'main';
// #co1
Swow\Coroutine::run(function () {
    ob_start();
    echo "foo\n";
    $ob_1 = (ob_get_status(true));
    // yield and it will switch to #co2
    msleep(2);
    // resume to ob_1
    Assert::same($ob_1, (ob_get_status(true)));
    ob_start(); // ob_2
    echo "bar\n";
    Assert::same(ob_get_status()['level'], 1);
    ob_start(); // ob_3
    // yield and it will switch to #co3
    msleep(3);
    // resume to ob_3
    Assert::same(ob_get_status()['level'], 2);
    echo "baz\n";
    Assert::same(ob_get_clean(), "baz\n"); // clean ob_3
    echo ob_get_clean(); // ob_1, ob_2, expect foo\n bar\n;
});

// #co2
Swow\Coroutine::run(function () {
    Assert::same(ob_get_status(true), []); //empty
    Assert::assert(!ob_get_contents());
    msleep(1);
    Assert::assert(!ob_get_clean());
});

// #co3
Swow\Coroutine::run(function () {
    msleep(3);
    Assert::same(ob_get_status(true), []); //empty
});
Assert::same(ob_get_clean(), 'main');

// TODO: WaitGroup

?>
--EXPECT--
foo
bar
