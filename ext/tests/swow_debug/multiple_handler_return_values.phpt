--TEST--
swow_debug: release every extended statement handler return value
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

use function Swow\Debug\registerExtendedStatementHandler;

$enabled = false;
$references = [];
$first = registerExtendedStatementHandler(static function () use (&$enabled, &$references): ?stdClass {
    if (!$enabled) {
        return null;
    }
    $object = new stdClass();
    $references[] = WeakReference::create($object);
    return $object;
}, true);
$second = registerExtendedStatementHandler(static function (): void {
}, true);
try {
    $enabled = true;
    $deadline = hrtime(true) + 1000000000;
    while (count($references) < 3 && hrtime(true) < $deadline) {
        Coroutine::getCurrent()->getId();
    }
    $enabled = false;
} finally {
    $enabled = false;
    $first->remove();
    $second->remove();
}
gc_collect_cycles();
Assert::notEmpty($references);
foreach ($references as $reference) {
    Assert::null($reference->get());
}
echo "Done\n";
?>
--EXPECT--
Done
