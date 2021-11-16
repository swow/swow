--TEST--
swow_coroutine: nested trace
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$wr = new WaitReference();

global $coroutine;

Coroutine::run(function () use ($wr) {
    Coroutine::run(function () use ($wr) {
        Coroutine::run(function () use ($wr) {
            Coroutine::run(function () use ($wr) {
                global $coroutine;
                $coroutine = Coroutine::run(function () use ($wr) {
                    echo (new Exception())->getTraceAsString() . PHP_LF . PHP_LF;
                    Coroutine::yield();
                    echo (new Exception())->getTraceAsString() . PHP_LF . PHP_LF;
                    usleep(1);
                    echo (new Exception())->getTraceAsString() . PHP_LF . PHP_LF;
                });
            });
        });
    });
});

$coroutine->resume();
WaitReference::wait($wr);

echo 'Done' . PHP_LF;

?>
--EXPECTF--
#0 [internal function]: {closure}()
#1 %s(%d): Swow\Coroutine::run(Object(Closure))
#2 [internal function]: {closure}()
#3 %s(%d): Swow\Coroutine::run(Object(Closure))
#4 [internal function]: {closure}()
#5 %s(%d): Swow\Coroutine::run(Object(Closure))
#6 [internal function]: {closure}()
#7 %s(%d): Swow\Coroutine::run(Object(Closure))
#8 [internal function]: {closure}()
#9 %s(%d): Swow\Coroutine::run(Object(Closure))
#10 {main}

#0 [internal function]: {closure}()
#1 %s(%d): Swow\Coroutine->resume()
#2 {main}

#0 [internal function]: {closure}()
#1 {main}

Done
