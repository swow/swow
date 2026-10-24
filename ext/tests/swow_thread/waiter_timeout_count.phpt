--TEST--
swow_thread_sync: a timed out waiter drops its count exactly once
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

use Swow\Semaphore;
use Swow\Thannel;

$semaphore = new Semaphore(0);
Swow\Coroutine::run(static function () use ($semaphore): void {
    $semaphore->acquire(10);
});
Swow\Coroutine::run(static function () use ($semaphore): void {
    $semaphore->acquire(30);
});
usleep(200000);

// the timeout callback takes the waiter out of the queue, and the waiter itself
// must not drop the count a second time
Assert::same(0, $semaphore->getWaiterCount());
$semaphore->release();
Assert::same(0, $semaphore->getWaiterCount());
unset($semaphore);

// a negative count also makes the destructor keep the object and warn
$thannel = new Thannel(0);
Swow\Coroutine::run(static function () use ($thannel): void {
    try {
        $thannel->pop(10);
    } catch (Swow\ThannelException) {
    }
});
Swow\Coroutine::run(static function () use ($thannel): void {
    try {
        $thannel->pop(20);
    } catch (Swow\ThannelException) {
    }
});
usleep(200000);
unset($thannel);

echo "done\n";
?>
--EXPECT--
done
