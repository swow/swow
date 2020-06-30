--TEST--
swow_coroutine: getTrace
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
$trace = $coroutine->getTrace();
echo (count($trace) === 5 ? 'Success' : 'Failed') . PHP_LF;
$coroutine->resume();

?>
--EXPECT--
Success
