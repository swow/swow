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

defer(function () {
    defer(function () {
        echo "a\n";
    });
    defer(function () {
        echo "b\n";
    });
    defer(function () {
        echo "c\n";
    });
    echo "1\n";
});
defer(function () {
    echo "2\n";
});
defer(function () {
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
