--TEST--
swow_coroutine: ref count
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(static function (): void {});
try {
    $coroutine = clone $coroutine;
} catch (Error $exception) {
    echo $exception->getMessage() . "\n";
}

class TestCoroutine extends Swow\Coroutine
{
    public function __clone()
    {
        var_dump(__FUNCTION__);
    }
}

$coroutine = new TestCoroutine(static function (): void {});
try {
    $coroutine = clone $coroutine;
} catch (Error $exception) {
    echo $exception->getMessage() . "\n";
}

?>
--EXPECT--
Trying to clone an uncloneable object of class Swow\Coroutine
Trying to clone an uncloneable object of class TestCoroutine
