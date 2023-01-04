--TEST--
swow_coroutine: getSwitches() and getGlobalSwitches()
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$coroutine = Coroutine::getCurrent();
Assert::same(Coroutine::getGlobalSwitches(), $coroutine->getGlobalSwitches());

$coroutineSwitches = $coroutine->getSwitches();
$remoteCoroutine = new Coroutine(static function () use ($coroutine, $coroutineSwitches): void {
    $remoteCoroutine = Coroutine::getCurrent();
    $remoteCoroutineSwitches = $remoteCoroutine->getSwitches();
    try {
        Assert::greaterThan($coroutine->getSwitches(), $coroutineSwitches);
    } finally {
        Coroutine::yield();
        Assert::greaterThan($remoteCoroutine->getSwitches(), $remoteCoroutineSwitches);
    }
});

$globalSwitches = Coroutine::getGlobalSwitches();
$remoteCoroutine->resume();
Assert::greaterThan(Coroutine::getGlobalSwitches(), $globalSwitches);

$globalSwitches = Coroutine::getGlobalSwitches();
$remoteCoroutine->resume();
Assert::greaterThan(Coroutine::getGlobalSwitches(), $globalSwitches);

echo "Done\n";
?>
--EXPECT--
Done
