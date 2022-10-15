--TEST--
swow_base: check if swow is loaded
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Assert::same(Swow::VERSION, sprintf(
    '%d.%d.%d%s%s',
    Swow::MAJOR_VERSION, Swow::MINOR_VERSION, Swow::RELEASE_VERSION,
    strlen(Swow::EXTRA_VERSION) > 0 ? '-' : '', Swow::EXTRA_VERSION
));
var_dump(Swow::VERSION);

?>
--EXPECTF--
string(%d) "%d.%d.%d%S"
