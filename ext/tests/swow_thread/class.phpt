--TEST--
swow_thread: class
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

var_dump(class_exists('Swow\Thread'));
var_dump(class_exists('Swow\ThreadException'));
var_dump(is_subclass_of('Swow\ThreadException', 'Throwable'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
