--TEST--
swow_debug: Debug\block() basic functionality
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Debug;

// Test 1: Basic blocking
$start = microtime(true);
Debug\block(100); // block for 100ms
$elapsed = (microtime(true) - $start) * 1000;

Assert::greaterThanEq($elapsed, 90); // allow 10% tolerance
echo "Block completed\n";

// Test 2: Invalid timeout (negative)
Assert::throws(static function (): void {
    Debug\block(-1);
}, Error::class);
echo "Negative timeout rejected\n";

// Test 3: Invalid timeout (zero)
Assert::throws(static function (): void {
    Debug\block(0);
}, Error::class);
echo "Zero timeout rejected\n";

echo "Done\n";

?>
--EXPECT--
Block completed
Negative timeout rejected
Zero timeout rejected
Done
