--TEST--
swow_coroutine: getCurrent
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () use (&$coroutine) {
    $coroutine_x = new Swow\Coroutine(function () { });
    $self = Swow\Coroutine::getCurrent();
    if ($self === $coroutine && $self !== $coroutine_x) {
        return 'Success';
    }
    return 'Failed';
});
echo 'Resume' . PHP_LF;
echo $coroutine->resume() . PHP_LF;
echo 'Done' . PHP_LF;

?>
--EXPECT--
Resume
Success
Done
