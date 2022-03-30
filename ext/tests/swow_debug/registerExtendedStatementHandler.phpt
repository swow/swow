--TEST--
swow_debug: registerExtendedStatementHandler
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('phar');
$ext_enable = '';
$loaded1 = shell_exec(PHP_BINARY . ' -m');
if (!str_contains($loaded1, 'Swow')) {
    $ext_enable .= ' -dextension=swow';
}
$loaded2 = shell_exec(PHP_BINARY . $ext_enable . ' -m');
if (
    !str_contains($loaded2, 'Swow') ||
    str_contains($loaded2, 'Warning')
) {
    skip('Swow is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension');
}
?>
--FILE--
<?php

// caller only
$ext_enable = '';
$loaded = shell_exec(PHP_BINARY . ' -m');
if (!str_contains($loaded, 'Swow')) {
    $ext_enable .= ' -dextension=swow';
}
passthru(PHP_BINARY . $ext_enable . ' ' . __DIR__ . DIRECTORY_SEPARATOR . 'registerExtendedStatementHandler.inc');
echo "\n";
passthru(PHP_BINARY . $ext_enable . ' -e ' . __DIR__ . DIRECTORY_SEPARATOR . 'registerExtendedStatementHandler.inc');
echo "\n";
passthru(PHP_BINARY . $ext_enable . ' -e ' . __DIR__ . DIRECTORY_SEPARATOR . 'registerExtendedStatementHandlerRemove.inc');
echo "\n";

echo "Done\n";
?>
--EXPECT--
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

Handler
Remove Handler
Handler
I am gone
Done

Done
