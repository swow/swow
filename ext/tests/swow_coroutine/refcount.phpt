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
        echo "In\n";
        $coroutine = new Swow\Coroutine(function () {
            echo "In sub\n";
            Swow\Coroutine::yield();
            echo "Out of sub\n";
        });
        echo "Resume sub 1\n";
        $coroutine->resume();
        Swow\Coroutine::yield();
        echo "Resume sub 2\n";
        $coroutine->resume();
        return 'Done';
    };
    $coroutine = new Swow\Coroutine($function);
    echo "Resume 1\n";
    $coroutine->resume();
    echo "Resume 2\n";
    echo $coroutine->resume() . "\n";
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
