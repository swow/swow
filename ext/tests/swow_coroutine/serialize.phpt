--TEST--
swow_coroutine: serialize
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
needs_php_version('>=', '8.1');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(static function (): void {});
try {
    serialize($coroutine);
} catch (Exception $exception) {
    echo $exception->getMessage() . "\n";
}

class TestCoroutine extends Swow\Coroutine
{
    public function __serialize(): array
    {
        return [];
    }
}

$coroutine = new TestCoroutine(static function (): void {});
try {
    serialize($coroutine);
} catch (Exception $exception) {
    echo $exception->getMessage() . "\n";
}

echo "Done\n";

?>
--EXPECT--
Serialization of 'Swow\Coroutine' is not allowed
Serialization of 'TestCoroutine' is not allowed
Done
