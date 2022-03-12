--TEST--
swow_defer: defer and exit
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use function Swow\defer;

Coroutine::run(function(){
    defer(function () {
        echo '1' . PHP_EOL;
    });
    defer(function () {
        echo '2' . PHP_EOL;
    });
    exit;
});

?>
--EXPECT--
2
1
