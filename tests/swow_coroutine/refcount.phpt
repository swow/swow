--TEST--
swow_coroutine: refcount
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

(function () {
    $function = function () {
        echo 'In' . PHP_LF;
        $coroutine = new Swow\Coroutine(function () {
            echo 'In sub' . PHP_LF;
            Swow\Coroutine::yield();
            echo 'Out of sub' . PHP_LF;
        });
        echo 'Resume sub 1' . PHP_LF;
        $coroutine->resume();
        Swow\Coroutine::yield();
        echo 'Resume sub 2' . PHP_LF;
        $coroutine->resume();
        return 'Done';
    };
    $coroutine = new Swow\Coroutine($function);
    echo 'Resume 1' . PHP_LF;
    $coroutine->resume();
    echo 'Resume 2' . PHP_LF;
    echo $coroutine->resume() . PHP_LF;
})();

?>
--EXPECT--
Resume 1
In
Resume sub 1
In sub
Resume 2
Resume sub 2
Out of sub
Done
