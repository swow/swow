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
use Swow\WatchDog;

Assert::false(WatchDog::isRunning());

WatchDog::run();

Assert::true(WatchDog::isRunning());

$coroutine = Coroutine::getCurrent();
$count = 0;

Coroutine::run(function () use ($coroutine, &$count) {
    sleep(0);
    Assert::greaterThan($count, 0);
    var_dump($count);
    $coroutine->throw(new RuntimeException('CPU starvation occurred'));
});

while (true) {
    $count++;
}

echo 'Never here' . PHP_LF;

?>
--EXPECTF--
int(%d)

Warning: [Fatal error in main] Uncaught %s: CPU starvation occurred in %s:%d
Stack trace:
#0 [internal function]: {closure}()
#1 {main}
  thrown in %s on line %d
