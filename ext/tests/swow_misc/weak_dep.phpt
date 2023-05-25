--TEST--
swow_misc: weak dependency
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
$loaded1 = shell_exec(PHP_BINARY . ' -m');
$ext_enable = '';
if (strpos($loaded1, 'Swow') === false) {
    $ext_enable .= ' ' . (getenv('TEST_PHP_ARGS') ?: '');
    $ext_enable .= ' ' . (getenv('TEST_PHP_EXTRA_ARGS') ?: '');
    $ext_enable = trim($ext_enable) ?: '-dextension=swow';
}
$loaded2 = shell_exec(PHP_BINARY . " {$ext_enable} -m");
if (
    strpos($loaded2, 'Swow') === false ||
    strpos($loaded2, 'Warning') !== false
) {
    skip('Swow is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension');
}
?>
--FILE--
<?php

// get extension dir
$extension_dir = shell_exec(PHP_BINARY . " -r \"echo ini_get('extension_dir');\"");

// get TEST_PHP_ARGS TEST_PHP_EXTRA_ARGS
$args = ' ' . (getenv('TEST_PHP_ARGS') ?: '');
$args .= ' ' . (getenv('TEST_PHP_EXTRA_ARGS') ?: '');

// remove pdo, make sure swow is loaded before any libpq dynamic lib
$args = preg_replace('/-d\s*"extension\s*=\s*pdo"/', '', $args);
$args = preg_replace('/-d\s*"extension\s*=\s*pdo_[^\s]*"/', '', $args);

// make sure only enable swow once
$args = preg_replace('/-d\s*"extension\s*=\s*swow"/', '', $args);
$args = preg_replace('/-d\s*extension=swow/', '', $args);

// enable swow
$args .= ' -dextension=swow';

// make sure custom extension_dir works
$args .= " -dextension_dir={$extension_dir}";

// start test
$loaded2 = shell_exec(PHP_BINARY . " -n {$args} -r \"echo Swow\\Extension::VERSION; echo 'hello Swow';\"");
if (
    strpos($loaded2, 'hello Swow') === false
) {
    echo "badresult: {$loaded2}\n";
}

?>
Done
--EXPECT--
Done
