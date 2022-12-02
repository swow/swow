--TEST--
swow_coroutine: resume vs yield
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
        Coroutine::getCurrent()->getPrevious()->resume();
        echo "4\n";
    });
    echo "3\n";
});
echo "5\n";

echo "\n";

Coroutine::run(static function (): void {
    echo "1\n";
    $coroutine = Coroutine::run(static function (): void {
        echo "2\n";
        Coroutine::yield();
        echo "4\n";
    });
    echo "3\n";
    $coroutine->resume();
    echo "5\n";
});
echo "6\n";

?>
--EXPECT--
1
2
3
4
5

1
2
3
4
5
6
