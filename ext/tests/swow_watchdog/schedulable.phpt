--TEST--
swow_watchdog: schedulable
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_in_valgrind();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Watchdog;

final class CoroutineContext
{
    public int $alertCount = 0;
}

final class CoroutineContextManager
{
    private static WeakMap $map;

    public static function getContext(Coroutine $coroutine)
    {
        self::$map ??= new WeakMap();
        return self::$map[$coroutine] ??= new CoroutineContext();
    }
}

Assert::false(Watchdog::isRunning());

Watchdog::run(1 * 1000 * 1000, 0, static function (): void {
    Assert::true(Watchdog::isRunning());
    $coroutine = Coroutine::getCurrent();
    $context = CoroutineContextManager::getContext($coroutine);
    $context->alertCount++;
    sleep(0);
    if ($context->alertCount > 5) {
        $coroutine->kill();
    }
});

$coroutine = Coroutine::getCurrent();
$count = 0;

Coroutine::run(static function () use ($coroutine, &$count): void {
    do {
        var_dump($count);
        sleep(0);
    } while ($coroutine->isAvailable());
    var_dump($count);
});

while (true) {
    $count++;
}

echo "Never here\n";

?>
--EXPECTREGEX--
int\(0\)
(?:int\((\d+)\)\n?)+
