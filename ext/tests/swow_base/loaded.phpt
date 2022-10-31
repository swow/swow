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

Assert::classExists(Swow::class);
Assert::classExists(Swow\Extension::class);
Assert::false(Swow::isLibraryLoaded());

?>
--EXPECT--
The extension "Swow" is available
