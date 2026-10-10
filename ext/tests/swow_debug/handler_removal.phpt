--TEST--
swow_debug: repeated handler removal preserves external ownership
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Utils\Handler;

use function Swow\Debug\registerExtendedStatementHandler;

foreach ([
    static fn(): Handler => Coroutine::registerDeadlockHandler(static function (): void {}),
    static fn(): Handler => registerExtendedStatementHandler(static function (): void {}, true),
] as $register) {
    $handler = $register();
    $reference = WeakReference::create($handler);
    $handler->remove();
    Assert::same($reference->get(), $handler);
    $handler->remove();
    Assert::same($reference->get(), $handler);
    unset($handler);
    gc_collect_cycles();
    Assert::null($reference->get());
}
echo "Done\n";
?>
--EXPECT--
Done
