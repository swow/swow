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
    serialize($coroutine);
} catch (Exception $exception) {
    echo $exception->getMessage() . PHP_LF;
}

class TestCoroutine extends Swow\Coroutine
{
    public function __sleep(): array
    {
        return [];
    }
}

$coroutine = new TestCoroutine(function () { });
try {
    serialize($coroutine);
} catch (Exception $exception) {
    echo $exception->getMessage() . PHP_LF;
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Serialization of 'Swow\Coroutine' is not allowed
Serialization of 'TestCoroutine' is not allowed
Done
