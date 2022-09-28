--TEST--
swow_coroutine: cross resume
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

Coroutine::run(static function (): void {
    echo "1\n";
    Coroutine::run(static function (): void {
        echo "2\n";
        Coroutine::run(static function (): void {
            echo "3\n";
            Coroutine::getCurrent()->getPrevious()->getPrevious()->resume();
            echo "5\n";
        });
        echo "6\n";
    });
    echo "4\n";
});
echo "7\n";

?>
--EXPECT--
1
2
3
4
5
6
7
