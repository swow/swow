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
var_dump(trim(php_exec_with_swow('-d allow_url_include=1 -r ""')));
var_dump(trim(php_exec_with_swow('-d allow_call_time_pass_reference=1 -r ""')));
var_dump(trim(php_exec_with_swow('-d asp_tags=1 -r ""')));

echo "Done\n";
?>
--EXPECT--
string(76) "Deprecated: Directive 'allow_url_include' is deprecated in Unknown on line 0"
string(106) "Fatal error: Directive 'allow_call_time_pass_reference' is no longer available in PHP in Unknown on line 0"
string(84) "Fatal error: Directive 'asp_tags' is no longer available in PHP in Unknown on line 0"
Done
