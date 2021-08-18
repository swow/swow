--TEST--
swow_misc: empty file
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

echo "Done\n";
?>
--EXPECT--
Done
