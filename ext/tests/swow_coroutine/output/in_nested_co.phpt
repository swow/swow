--TEST--
swow_coroutine/output: use ob_* in nest co
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

// FIXME: memory leak
Swow\Coroutine::run(function () {
    ob_start();
    echo "2\n"; // [#1] yield
    Swow\Coroutine::run(function () {
        echo "1\n"; // [#2] output: 1
        sleep(0); // yield
        // [#4] resume
        ob_start(); // to buffer
        echo "4\n";
    }); // [#5] destroyed and output: 4
    echo "3\n";
}); // [#3] destroyed and output: 2 3
sleep(0);

echo 'Done' . PHP_LF;

?>
--EXPECT--
1
2
3
4
Done
