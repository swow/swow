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

$coroutine = new Coroutine(function ($a, $b) {
    $c = Coroutine::yield($a + $b);
    $d = sub1();
    return $d . ' ' . $c;
});

echo $coroutine->resume(1, 2) . PHP_LF;
try {
    echo $coroutine->resume('hello', 'world') . PHP_LF;
} catch (Error $error) {
    echo $coroutine->resume('world') . PHP_LF;
    echo $coroutine->resume('hello') . PHP_LF;
}

?>
--EXPECT--
3
1
hello world
