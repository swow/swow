--TEST--
swow_coroutine: getTraceAsString
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(function () {
    (function () {
        (function () {
            (function () {
                Swow\Coroutine::yield();
            })();
        })();
    })();
});
echo $coroutine->getTraceAsString() . PHP_LF;
$coroutine->resume();

?>
--EXPECTF--
#0 %s(%d): Swow\Coroutine::yield()
#1 %s(%d): {closure}()
#2 %s(%d): {closure}()
#3 %s(%d): {closure}()
#4 [internal function]: {closure}()
#5 {main}
