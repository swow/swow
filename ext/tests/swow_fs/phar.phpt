--TEST--
swow_fs: read code in phar
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('phar');
$ext_enable = '';
$loaded1 = shell_exec(PHP_BINARY . ' -m');
if (strpos($loaded1, 'Swow') === false) {
    $ext_enable .= ' -dextension=swow';
}
if (strpos($loaded1, 'Phar') === false) {
    $ext_enable .= ' -dextension=phar';
}
$loaded2 = shell_exec(PHP_BINARY . $ext_enable . ' -m');
if (
    strpos($loaded2, 'Phar') === false ||
    strpos($loaded2, 'Swow') === false ||
    strpos($loaded2, 'Warning') !== false
) {
    skip('Swow or phar is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension');
}
?>
--INI--
phar.readonly=0
--FILE--
<?php

require __DIR__ . '/../include/bootstrap.php';
require_once __DIR__ . '/build_phar.inc';

$ext_enable = ' ';
$loaded = shell_exec(PHP_BINARY . ' -m');
if (strpos($loaded, 'Swow') === false) {
    $ext_enable = ' -dextension=swow ';
}

build_phar(__DIR__ . DIRECTORY_SEPARATOR . 'phartest.phar', TEST_AUTOLOAD);
rename(__DIR__ . DIRECTORY_SEPARATOR . 'phartest.phar', __DIR__ . DIRECTORY_SEPARATOR . 'phartest');
var_dump(php_exec_with_swow(__DIR__ . DIRECTORY_SEPARATOR . 'phartest'));

build_phar(__DIR__ . DIRECTORY_SEPARATOR . 'phartest2.phar', TEST_REQUIRE);
rename(__DIR__ . DIRECTORY_SEPARATOR . 'phartest2.phar', __DIR__ . DIRECTORY_SEPARATOR . 'phartest2');
var_dump(php_exec_with_swow(__DIR__ . DIRECTORY_SEPARATOR . 'phartest2'));

build_phar(__DIR__ . DIRECTORY_SEPARATOR . 'phartest3.phar', TEST_INCLUDE);
rename(__DIR__ . DIRECTORY_SEPARATOR . 'phartest3.phar', __DIR__ . DIRECTORY_SEPARATOR . 'phartest3');
var_dump(php_exec_with_swow(__DIR__ . DIRECTORY_SEPARATOR . 'phartest3'));

echo "Done\n";
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/phartest.phar');
@unlink(__DIR__ . '/phartest');
@unlink(__DIR__ . '/phartest2.phar');
@unlink(__DIR__ . '/phartest2');
@unlink(__DIR__ . '/phartest3.phar');
@unlink(__DIR__ . '/phartest3');
?>
--EXPECT--
string(117) "on run.php
Class1 initializing
Class1 run() is called
Class2 initializing
Class2 run() is called
Class1 initializing
"
string(117) "on run.php
Class1 initializing
Class1 run() is called
Class2 initializing
Class2 run() is called
Class1 initializing
"
string(117) "on run.php
Class1 initializing
Class1 run() is called
Class2 initializing
Class2 run() is called
Class1 initializing
"
Done