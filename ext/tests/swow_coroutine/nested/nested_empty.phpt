--TEST--
swow_coroutine/nested: nested empty
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

Swow\Coroutine::run(static function (): void {
    echo "coroutine[1] start\n";
    Swow\Coroutine::run(static function (): void {
        echo "coroutine[2] start\n";
        echo "coroutine[2] exit\n";
    });
    echo "coroutine[1] exit\n";
});

?>
--EXPECT--
coroutine[1] start
coroutine[2] start
coroutine[2] exit
coroutine[1] exit
