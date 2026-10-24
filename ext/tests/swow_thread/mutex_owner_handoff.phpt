--TEST--
swow_mutex: the owner moves to the waiter that takes the mutex
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Mutex;
use Swow\SyncException;

$mutex = new Mutex();
Assert::true($mutex->lock());

$handedOver = false;
Swow\Coroutine::run(static function () use ($mutex, &$handedOver): void {
    Assert::true($mutex->lock(-1));
    usleep(10000);
    // the owner is this thread now, so the release has to succeed
    $mutex->unlock();
    $handedOver = true;
});

usleep(2000);
$mutex->unlock();

usleep(60000);
Assert::true($handedOver);

// a release that erased the new owner would leave the mutex locked forever
Assert::true($mutex->lock(0));
$mutex->unlock();
Assert::throws(static function () use ($mutex): void {
    $mutex->unlock();
}, SyncException::class);

echo "done\n";
?>
--EXPECT--
done
