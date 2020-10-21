--TEST--
swow_coroutine: throw
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(function () {
    try {
        Swow\Coroutine::yield();
        return 'Never here';
    } catch (Throwable $throwable) {
        return get_class($throwable);
    }
});
var_dump($coroutine->throw(new Exception));

?>
--EXPECT--
string(9) "Exception"
