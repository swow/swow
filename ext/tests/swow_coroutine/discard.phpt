--TEST--
swow_coroutine: discard
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

(static function (): void {
    $function = static function (): void {
        echo "In\n";
        $leak = new stdClass();
        Swow\Coroutine::yield();
        echo "Never here\n";
        var_dump($leak);
    };
    $coroutine = new Swow\Coroutine($function);
    $coroutine->resume();
    $coroutine->kill();
})();
echo "Done\n";

?>
--EXPECT--
In
Done
