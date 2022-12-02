--TEST--
swow_coroutine: extends and run
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

class TestCoroutine extends Swow\Coroutine
{
}

$coroutine = TestCoroutine::run(static function (): void {
    var_dump(get_class(Swow\Coroutine::getCurrent()));
});

echo "Done\n";
?>
--EXPECT--
string(13) "TestCoroutine"
Done
