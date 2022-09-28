--TEST--
swow_defer: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\defer;

defer(static function (): void {
    defer(static function (): void {
        echo "a\n";
    });
    defer(static function (): void {
        echo "b\n";
    });
    defer(static function (): void {
        echo "c\n";
    });
    echo "1\n";
});
defer(static function (): void {
    echo "2\n";
});
defer(static function (): void {
    echo "3\n";
});

echo "Done\n";

?>
--EXPECT--
Done
3
2
1
c
b
a
