--TEST--
swow_coroutine: resume current
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\CoroutineException;

$coroutine = new Coroutine(static function () use (&$coroutine): void {
    echo "In\n";
    Assert::throws(static function () use ($coroutine): void {
        $coroutine->resume();
    }, CoroutineException::class);
    echo "Out\n";
});
echo "Resume\n";
$coroutine->resume();

echo "Done\n";

?>
--EXPECT--
Resume
In
Out
Done
