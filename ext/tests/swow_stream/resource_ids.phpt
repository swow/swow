--TEST--
swow_stream: Swow takes no resource ids before the script runs
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_SAPI !== 'cli', 'STDIN is defined by the CLI only');
?>
--FILE--
<?php
// scripts and tests print resource ids; they must be the ones of native PHP
var_dump(STDIN, STDOUT, STDERR);
?>
--EXPECT--
resource(1) of type (stream)
resource(2) of type (stream)
resource(3) of type (stream)
