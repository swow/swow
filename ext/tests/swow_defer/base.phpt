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
        echo 'a' . PHP_LF;
    });
    defer(function () {
        echo 'b' . PHP_LF;
    });
    defer(function () {
        echo 'c' . PHP_LF;
    });
    echo '1' . PHP_LF;
});
defer(function () {
    echo '2' . PHP_LF;
});
defer(function () {
    echo '3' . PHP_LF;
});

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
3
2
1
c
b
a
