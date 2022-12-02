--TEST--
swow_log: basic functionality
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Log;

$types = Log::getTypes();
Assert::integer($types);
Log::setTypes($types);

echo "Done\n";
?>
--EXPECT--
Done
