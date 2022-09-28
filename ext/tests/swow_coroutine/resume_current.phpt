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

$coroutine = new Coroutine(function () use (&$coroutine) {
    echo "In\n";
    Assert::throws(function () use ($coroutine) {
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
