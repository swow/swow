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
                    echo Coroutine::getCurrent()->getTraceAsString() . PHP_LF . PHP_LF;
                    Coroutine::yield();
                    echo Coroutine::getCurrent()->getTraceAsString() . PHP_LF . PHP_LF;
                    usleep(1);
                    echo Coroutine::getCurrent()->getTraceAsString() . PHP_LF . PHP_LF;
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
#%d %s(%d): Swow\Coroutine->getTraceAsString()
#%d [internal function]: {closure}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closure}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closure}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closure}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closure}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}

#%d %s(%d): Swow\Coroutine->getTraceAsString()
#%d [internal function]: {closure}()%A
#%d %s(%d): Swow\Coroutine->resume()
#%d {main}

#%d %s(%d): Swow\Coroutine->getTraceAsString()
#%d [internal function]: {closure}()%A
#%d {main}

Done
