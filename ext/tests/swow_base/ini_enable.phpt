--TEST--
swow_base: check swow.enable work
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.enable=0;
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

// should not be settable
Assert::false(ini_set('swow.enable', '1'));
Assert::same(ini_get('swow.enable'), '0');
Assert::false(ini_set('swow.enable', 'On'));
Assert::same(ini_get('swow.enable'), '0');

Assert::true(extension_loaded('swow'));
Assert::false(class_exists('Swow'));
Assert::false(class_exists('Swow\Socket'));

echo "Done\n";
?>
--EXPECT--
Done
