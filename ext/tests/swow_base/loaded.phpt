--TEST--
swow_base: check if swow is loaded
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

echo "The extension \"Swow\" is available\n";

?>
--EXPECT--
The extension "Swow" is available
