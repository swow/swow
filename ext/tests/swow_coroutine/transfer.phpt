--TEST--
swow_coroutine: transfer
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

function sub1()
{
    return Coroutine::yield(1);
}

$coroutine = new Coroutine(static function ($a, $b) {
    $c = Coroutine::yield($a + $b);
    $d = sub1();
    return $d . ' ' . $c;
});

echo $coroutine->resume(1, 2) . "\n";
try {
    echo $coroutine->resume('hello', 'world') . "\n";
} catch (Error $error) {
    echo $coroutine->resume('world') . "\n";
    echo $coroutine->resume('hello') . "\n";
}

?>
--EXPECT--
3
1
hello world
