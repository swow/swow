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
    echo 'In' . PHP_LF;
    Assert::throws(function () use ($coroutine) {
        $coroutine->resume();
    }, CoroutineException::class);
    echo 'Out' . PHP_LF;
});
echo 'Resume' . PHP_LF;
$coroutine->resume();

echo 'Done' . PHP_LF;

?>
--EXPECT--
Resume
In
Out
Done
