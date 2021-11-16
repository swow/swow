--TEST--
swow_coroutine: resume current
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () use (&$coroutine) {
    echo 'In' .PHP_LF;
    $coroutine->resume();
    echo 'Never here' . PHP_LF;
});
echo 'Resume' . PHP_LF;
$coroutine->resume();

echo 'Done' . PHP_LF;

?>
--EXPECTF--
Resume
In

Warning: [Fatal error in R%d] Uncaught Swow\Coroutine\Exception: Coroutine is already running in %s:%d
Stack trace:
#0 %s(%d): Swow\Coroutine->resume()
#1 [internal function]: {closure}()
#2 %s(%d): Swow\Coroutine->resume()
#3 {main}
  thrown in %s on line %d
Done
