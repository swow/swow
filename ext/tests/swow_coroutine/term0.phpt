--TEST--
swow_coroutine: term(0)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(function () {
    Swow\Coroutine::yield();
    echo 'Never here';
});
$coroutine->term();
echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
