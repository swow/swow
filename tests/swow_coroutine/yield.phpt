--TEST--
swow_coroutine: yield
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () {
    echo 'In' . PHP_LF;
    Swow\Coroutine::yield();
    echo 'End' . PHP_LF;
});
echo 'Resume' . PHP_LF;
$coroutine->resume();
echo 'Out' . PHP_LF;
$coroutine->resume();
echo 'Done' . PHP_LF;

?>
--EXPECT--
Resume
In
Out
End
Done
