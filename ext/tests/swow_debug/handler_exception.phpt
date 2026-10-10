--TEST--
swow_debug: propagate handler exceptions without calling later handlers
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

use function Swow\Debug\registerExtendedStatementHandler;

$throw = false;
$secondCalls = 0;
$secondCallsAtThrow = null;
$secondCallsAtCatch = null;
$first = registerExtendedStatementHandler(static function () use (&$throw, &$secondCalls, &$secondCallsAtThrow): void {
    if ($throw) {
        $throw = false;
        $secondCallsAtThrow = $secondCalls;
        throw new RuntimeException('handler-exception-marker');
    }
}, true);
$second = registerExtendedStatementHandler(static function () use (&$secondCalls): void {
    $secondCalls++;
}, true);
$caught = null;
try {
    $secondCalls = 0;
    $throw = true;
    Coroutine::getCurrent()->getId();
} catch (RuntimeException $exception) {
    $secondCallsAtCatch = $secondCalls;
    $caught = $exception;
} finally {
    $first->remove();
    $second->remove();
}
Assert::same($caught?->getMessage(), 'handler-exception-marker');
Assert::same($secondCallsAtCatch, $secondCallsAtThrow);
echo "Done\n";
?>
--EXPECT--
Done
