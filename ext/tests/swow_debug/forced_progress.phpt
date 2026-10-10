--TEST--
swow_debug: forced observation makes progress through loops and finally
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.debug_force_interval=2
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\Debug\block;
use function Swow\Debug\registerExtendedStatementHandler;

for ($round = 0; $round < 3; $round++) {
    $calls = 0;
    $invocations = 0;
    $enabled = false;
    $handler = registerExtendedStatementHandler(static function () use (&$calls, &$invocations, &$enabled): void {
        $invocations++;
        if ($enabled) {
            $calls++;
            // A callback longer than the interval must not request its own interrupt.
            block(5);
        }
    }, true);
    $iterations = 0;
    $finallyReached = false;
    try {
        $enabled = true;
        $deadline = hrtime(true) + 1000000000;
        while ($calls < 3 && hrtime(true) < $deadline) {
            $iterations++;
        }
        Assert::greaterThanEq($calls, 3);
    } finally {
        $finallyReached = true;
        $enabled = false;
        $handler->remove();
    }
    Assert::true($finallyReached);
    Assert::greaterThan($iterations, 0);
    $callsAfterRemoval = $invocations;
    $deadline = hrtime(true) + 10000000;
    while (hrtime(true) < $deadline) {
        $iterations++;
    }
    Assert::same($invocations, $callsAfterRemoval);
}
echo "Done\n";
?>
--EXPECT--
Done
