--TEST--
swow_defer: defer and exit
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\defer;

defer(function () {
    echo '1' . PHP_LF;
});
defer(function () {
    echo '2' . PHP_LF;
});

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
2
1
