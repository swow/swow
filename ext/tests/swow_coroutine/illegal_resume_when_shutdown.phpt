--TEST--
swow_coroutine: illegal resume when shutdown
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () { });
set_exception_handler(function () use ($coroutine) {
    echo "Never here\n";
    $coroutine->resume();
});
(function () {
    $coroutine = Swow\Coroutine::run(function () {
        Swow\Coroutine::yield();
        echo "Never here\n";
    });
    $coroutine->kill();
})();

echo "Done\n";

?>
--EXPECT--
Done
