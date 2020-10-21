--TEST--
swow_coroutine/output: main output global
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

ob_start();
echo 'aaa';
Swow\Coroutine::run(function () {
    ob_start();
    echo 'bbb';
    sleep(0);
    echo ob_get_clean() . PHP_LF;
});
echo ob_get_clean() . PHP_LF;

// TODO: WaitGroup

?>
--EXPECT--
aaa
bbb
