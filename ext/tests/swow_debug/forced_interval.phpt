--TEST--
swow_debug: reject a nonpositive forced observation interval
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.debug_force_interval=0
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\Debug\registerExtendedStatementHandler;

try {
    registerExtendedStatementHandler(static function (): void {}, true);
} catch (Error $error) {
    Assert::same($error->getMessage(), 'swow.debug_force_interval must be a positive number of milliseconds within the timer range');
    echo "Done\n";
}
?>
--EXPECT--
Done
