--TEST--
swow_coroutine: kill main by self
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Swow\Coroutine::getCurrent()->kill();

echo 'Never here and keep silent' . PHP_LF;

?>
--EXPECT--
