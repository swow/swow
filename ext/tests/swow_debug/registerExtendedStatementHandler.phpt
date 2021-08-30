--TEST--
swow_debug: registerExtendedStatementHandler
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('phar');
$ext_enable = '';
$loaded1 = shell_exec(PHP_BINARY . ' -m');
if (strpos($loaded1, 'Swow') === false) {
    $ext_enable .= ' -dextension=swow';
}
$loaded2 = shell_exec(PHP_BINARY . $ext_enable . ' -m');
if (
    strpos($loaded2, 'Swow') === false ||
    strpos($loaded2, 'Warning') !== false
) {
    skip('Swow is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension');
}
?>
--FILE--
<?php

// caller only
$ext_enable = '';
$loaded = shell_exec(PHP_BINARY . ' -m');
if (strpos($loaded, 'Swow') === false) {
    $ext_enable .= ' -dextension=swow';
}
passthru(PHP_BINARY . $ext_enable . ' ' . __DIR__ . DIRECTORY_SEPARATOR . 'registerExtendedStatementHandler.inc');
passthru(PHP_BINARY . $ext_enable . ' -e ' . __DIR__ . DIRECTORY_SEPARATOR . 'registerExtendedStatementHandler.inc');

echo "Done\n";
?>
--EXPECTF--
string(43) "Please re-run your program with "-e" option"
array(2) {
  ["i"]=>
  int(11)
  ["some_var"]=>
  int(42)
}
string(8) "break me"
int(0)
Done
