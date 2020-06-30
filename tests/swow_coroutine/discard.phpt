--TEST--
swow_coroutine: discard
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
        $leak = new stdClass;
        Swow\Coroutine::yield();
        echo 'Never here' . PHP_LF;
        var_dump($leak);
    };
    $coroutine = new Swow\Coroutine($function);
    $coroutine->resume();
    $coroutine->kill();
})();
echo 'Done' . PHP_LF;

?>
--EXPECT--
In
Done
