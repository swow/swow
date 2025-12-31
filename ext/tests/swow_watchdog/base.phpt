--TEST--
swow_watchdog: base
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

Assert::false(Watchdog::isRunning());

Watchdog::run();

Assert::true(Watchdog::isRunning());

$coroutine = Coroutine::getCurrent();
$count = 0;

Coroutine::run(static function () use ($coroutine, &$count): void {
    sleep(0);
    Assert::greaterThan($count, 0);
    var_dump($count);
    $coroutine->throw(new RuntimeException('CPU starvation occurred'));
});

while (true) {
    $count++;
}

echo "Never here\n";

?>
--EXPECTREGEX--
(?:Warning:[^\n]*\n)?int\(\d+\)

Warning: \[Fatal error in main\] Uncaught .+: CPU starvation occurred in .+:\d+
Stack trace:
#0 \[internal function\]: \{closur.+\}\(\)
#1 \{main\}
  thrown in .+ on line \d+
