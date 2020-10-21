--TEST--
swow_coroutine: getPrevious
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$main = Swow\Coroutine::getCurrent();
$coroutine = new Swow\Coroutine(function () use ($main, &$coroutine) {
    if ($main === $coroutine->getPrevious()) {
        echo 'Success' . PHP_LF;
    }
    echo 'Yield' . PHP_LF;
    $main->resume();
    echo 'End' . PHP_LF;
});
echo 'Resume-1' . PHP_LF;
$coroutine->resume();
echo 'Resume-2' . PHP_LF;
$coroutine->resume();
echo 'Done' . PHP_LF;

?>
--EXPECT--
Resume-1
Success
Yield
Resume-2
End
Done
