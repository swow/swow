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
$fatalErrorOptions = PHP_VERSION_ID >= 80500 ? '-d fatal_error_backtraces=1 ' : '';
$output = trim(php_exec_with_swow($fatalErrorOptions . '-d allow_call_time_pass_reference=1 -r ""'));
$fatalErrorSuffix = PHP_VERSION_ID >= 80500 ? "\nStack trace:\n#0 {main}" : '';
Assert::eq($output, "Fatal error: Directive 'allow_call_time_pass_reference' is no longer available in PHP in Unknown on line 0" . $fatalErrorSuffix);
$output = trim(php_exec_with_swow($fatalErrorOptions . '-d asp_tags=1 -r ""'));
Assert::eq($output, "Fatal error: Directive 'asp_tags' is no longer available in PHP in Unknown on line 0" . $fatalErrorSuffix);

echo "Done\n";
?>
--EXPECT--
Done
