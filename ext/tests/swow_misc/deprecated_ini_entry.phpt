--TEST--
swow_misc: deprecated ini entries
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_cannot_make_subprocess();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
$output = trim(php_exec_with_swow('-d allow_url_include=1 -r ""'));
Assert::eq($output, "Deprecated: Directive 'allow_url_include' is deprecated in Unknown on line 0");
$output = trim(php_exec_with_swow('-d allow_call_time_pass_reference=1 -r ""'));
if (PHP_VERSION_ID < 80600) {
    Assert::eq($output, "Fatal error: Directive 'allow_call_time_pass_reference' is no longer available in PHP in Unknown on line 0");
} else {
    Assert::true(str_starts_with($output, "Fatal error: Directive 'allow_call_time_pass_reference' is no longer available in PHP in Unknown on line 0"));
}
$output = trim(php_exec_with_swow('-d asp_tags=1 -r ""'));
if (PHP_VERSION_ID < 80600) {
    Assert::eq($output, "Fatal error: Directive 'asp_tags' is no longer available in PHP in Unknown on line 0");
} else {
    Assert::true(str_starts_with($output, "Fatal error: Directive 'asp_tags' is no longer available in PHP in Unknown on line 0"));
}

echo "Done\n";
?>
--EXPECT--
Done
