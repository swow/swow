--TEST--
swow_coroutine: ref count
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () { });
try {
    $coroutine = clone $coroutine;
} catch (Error $exception) {
    echo $exception->getMessage() . PHP_LF;
}

class TestCoroutine extends Swow\Coroutine
{
    public function __clone()
    {
        var_dump(__FUNCTION__);
    }
}

$coroutine = new TestCoroutine(function () { });
try {
    $coroutine = clone $coroutine;
} catch (Error $exception) {
    echo $exception->getMessage() . PHP_LF;
}

?>
--EXPECT--
Trying to clone an uncloneable object of class Swow\Coroutine
Trying to clone an uncloneable object of class TestCoroutine
