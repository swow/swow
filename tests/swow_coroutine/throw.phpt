--TEST--
swow_coroutine: throw
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () {
    try {
        Swow\Coroutine::yield();
    } catch (Throwable $throwable) {
        var_dump(get_class($throwable));
    }
});
$coroutine->resume();
$coroutine->throw(new Exception);

?>
--EXPECT--
string(9) "Exception"
