--TEST--
swow_base: check if swow is loaded
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Assert::same(Swow\Extension::VERSION, sprintf(
    '%d.%d.%d%s%s',
    Swow\Extension::MAJOR_VERSION,
    Swow\Extension::MINOR_VERSION,
    Swow\Extension::RELEASE_VERSION,
    strlen(Swow\Extension::EXTRA_VERSION) > 0 ? '-' : '', Swow\Extension::EXTRA_VERSION
));
Assert::same(Swow\Extension::VERSION_ID, (int) sprintf(
    '%02d%02d%02d',
    Swow\Extension::MAJOR_VERSION,
    Swow\Extension::MINOR_VERSION,
    Swow\Extension::RELEASE_VERSION
));

var_dump(Swow\Extension::VERSION);
var_dump(Swow\Extension::VERSION_ID);

?>
--EXPECTF--
string(%d) "%d.%d.%d%S"
int(%d)
