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
// Released PHP 8.5 versions append a main-frame stack trace to a fatal startup error, while
// builds from the current PHP-8.5 branch and master omit it. Accept exactly these two forms,
// and no other suffix.
$stripMainFrameTrace = static function (string $output): string {
    $trace = "\nStack trace:\n#0 {main}";
    if (PHP_VERSION_ID >= 80500 && str_ends_with($output, $trace)) {
        return substr($output, 0, -strlen($trace));
    }

    return $output;
};
Assert::eq($stripMainFrameTrace($output), "Fatal error: Directive 'allow_call_time_pass_reference' is no longer available in PHP in Unknown on line 0");
$output = trim(php_exec_with_swow($fatalErrorOptions . '-d asp_tags=1 -r ""'));
Assert::eq($stripMainFrameTrace($output), "Fatal error: Directive 'asp_tags' is no longer available in PHP in Unknown on line 0");

echo "Done\n";
?>
--EXPECT--
Done
