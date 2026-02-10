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

Coroutine::run(static function () use ($wr): void {
    Coroutine::run(static function () use ($wr): void {
        Coroutine::run(static function () use ($wr): void {
            Coroutine::run(static function () use ($wr): void {
                global $coroutine;
                $coroutine = Coroutine::run(static function () use ($wr): void {
                    echo Coroutine::getCurrent()->getTraceAsString() . "\n\n";
                    Coroutine::yield();
                    echo Coroutine::getCurrent()->getTraceAsString() . "\n\n";
                    usleep(1);
                    echo Coroutine::getCurrent()->getTraceAsString() . "\n\n";
                });
            });
        });
    });
});

$coroutine->resume();
WaitReference::wait($wr);

echo "Done\n";

?>
--EXPECTF--
#%d %s(%d): Swow\Coroutine->getTraceAsString()
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}

#%d %s(%d): Swow\Coroutine->getTraceAsString()
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine->resume()
#%d {main}

#%d %s(%d): Swow\Coroutine->getTraceAsString()
#%d [internal function]: {closur%s}()%A
#%d {main}

Done
