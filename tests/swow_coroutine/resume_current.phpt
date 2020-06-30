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
    echo 'Never here';
});
echo 'Resume' . PHP_LF;
$coroutine->resume();

?>
--EXPECTF--
Resume
In

Warning: [Fatal error in R2] Uncaught Swow\Coroutine\Exception: Coroutine is running in %s:%d
Stack trace:
#0 %s(%d): Swow\Coroutine->resume()
#1 [internal function]: {closure}()
#2 {main}
  thrown in %s on line %d
