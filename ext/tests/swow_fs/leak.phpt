--TEST--
swow_fs: leak
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_DEBUG !== 1, 'Debug build only');
?>
--XFAIL--
Need to fix
--FILE--
<?php
opendir(__DIR__);
echo 'Done' . PHP_EOL;
?>
--EXPECT--
Done
