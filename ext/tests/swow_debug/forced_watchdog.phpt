--TEST--
swow_debug: forced observation preserves watchdog interrupts
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

use function Swow\Debug\registerExtendedStatementHandler;

foreach ([false, true] as $watchdogFirst) {
    $samples = 0;
    $alerts = 0;
    $startWatchdog = static function () use (&$alerts): void {
        Watchdog::run(10000000, 100000000, static function (string $type) use (&$alerts): void {
            if ($type === 'cpu') {
                $alerts++;
                sleep(0);
            }
        });
    };
    if ($watchdogFirst) {
        $startWatchdog();
    }
    $handler = registerExtendedStatementHandler(static function () use (&$samples): void {
        $samples++;
    }, true);
    if (!$watchdogFirst) {
        $startWatchdog();
    }
    // Keep another coroutine available so the watchdog checks CPU starvation.
    $other = Coroutine::run(static function (): void {
        sleep(1);
    });
    try {
        $deadline = hrtime(true) + 1000000000;
        while (($samples < 3 || $alerts === 0) && hrtime(true) < $deadline) {
        }
        Assert::greaterThanEq($samples, 3);
        Assert::greaterThan($alerts, 0);
    } finally {
        Watchdog::stop();
        $handler->remove();
        $other->kill();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
