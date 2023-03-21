--TEST--
swow_misc: weak dependency
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
$loaded1 = shell_exec(PHP_BINARY . ' -m');
$ext_enable = "";
if (strpos($loaded1, 'Swow') === false) {
    $ext_enable .= " " . (getenv("TEST_PHP_ARGS") ?: "");
    $ext_enable .= " " . (getenv("TEST_PHP_EXTRA_ARGS") ?: "");
    $ext_enable = trim($ext_enable) ?: "-dextension=swow";
}
$loaded2 = shell_exec(PHP_BINARY . " $ext_enable -m");
if (
    strpos($loaded2, 'Swow') === false ||
    strpos($loaded2, 'Warning') !== false
) {
    skip('Swow is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension');
}
?>
--FILE--
<?php

$loaded1 = shell_exec(PHP_BINARY . ' -n -m');
$ext_enable = "";
if (strpos($loaded1, 'Swow') === false) {
    $ext_enable .= " " . (getenv("TEST_PHP_ARGS") ?: "");
    $ext_enable .= " " . (getenv("TEST_PHP_EXTRA_ARGS") ?: "");
    $ext_enable = trim($ext_enable) ?: "-dextension=swow";
}
$ext_enable = preg_replace('/-d\s*"extension\s*=\s*pdo"/', '', $ext_enable);
$ext_enable = preg_replace('/-d\s*"extension\s*=\s*pdo_[^\s]*"/', '', $ext_enable);
$loaded2 = shell_exec(PHP_BINARY . " -n $ext_enable -r 'echo \"hello Swow\";'");
if (
    strpos($loaded2, 'hello Swow') === false
) {
    print("badresult: $loaded2\n");
}

?>
Done
--EXPECT--
Done
