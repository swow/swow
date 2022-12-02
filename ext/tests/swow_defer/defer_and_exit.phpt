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

Coroutine::run(static function (): void {
    defer(static function (): void {
        echo '1' . PHP_EOL;
    });
    defer(static function (): void {
        echo '2' . PHP_EOL;
    });
    exit;
});

?>
--EXPECT--
2
1
