--TEST--
swow_coroutine: term(0) main
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Swow\Coroutine::getCurrent()->term();

?>
--EXPECT--
