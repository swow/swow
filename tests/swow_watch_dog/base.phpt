--TEST--
swow_watch_dog: base
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

WatchDog::run();

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

?>
--EXPECTF--
int(%d)

Warning: [Fatal error in R1] Uncaught %s: CPU starvation occurred in %s:%d
Stack trace:
#0 [internal function]: {closure}()
#1 {main}

Next %s: CPU starvation occurred in %s:%d
Stack trace:
#0 {main}
  thrown in %s on line %d
