--TEST--
swow_base: check swow.async_xxx unsettable
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

?>
--INI--
swow.async_threads=123;
swow.async_file=0;
swow.async_tty=0;
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

// should not be settable
Assert::false(ini_set('swow.async_threads', '122'));
Assert::same(ini_get('swow.async_threads'), '123');
Assert::false(ini_set('swow.async_file', '1'));
Assert::same(ini_get('swow.async_file'), '0');
Assert::false(ini_set('swow.async_tty', '1'));
Assert::same(ini_get('swow.async_tty'), '0');

echo "Done\n";
?>
--EXPECT--
Done
