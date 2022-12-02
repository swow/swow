--TEST--
swow_coroutine/output: main output global
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

ob_start();
echo "0\n";
Swow\Coroutine::run(static function (): void {
    ob_start();
    echo "1\n";
    Swow\Coroutine::run(static function (): void {
        ob_start();
        echo "2\n";
    }); // close 2
}); // close 1
// close 0

echo "Done\n";

?>
--EXPECT--
2
1
0
Done
