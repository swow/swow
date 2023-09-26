--TEST--
swow_coroutine: illegal resume when shutdown
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(static function (): void {});
set_exception_handler(static function () use ($coroutine): void {
    echo "Never here\n";
    $coroutine->resume();
});
(static function (): void {
    $coroutine = Swow\Coroutine::run(static function (): void {
        Swow\Coroutine::yield();
        echo "Never here\n";
    });
    $coroutine->kill();
})();

echo "Done\n";

?>
--EXPECT--
Done
