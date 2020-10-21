--TEST--
swow_coroutine: resume
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () {
    echo 'Hello Swow' . PHP_LF;
});
echo 'Resume' . PHP_LF;
$coroutine->resume();
echo 'Done' . PHP_LF;

?>
--EXPECT--
Resume
Hello Swow
Done
