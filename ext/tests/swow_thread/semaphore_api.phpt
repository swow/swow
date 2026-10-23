--TEST--
swow_thread: Semaphore API
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Semaphore;

$semaphore = new Semaphore(2);
Assert::same(2, $semaphore->getValue());
Assert::same(0, $semaphore->getWaiterCount());
Assert::true($semaphore->tryAcquire());
Assert::same(1, $semaphore->getValue());
Assert::true($semaphore->acquire(0));
Assert::same(0, $semaphore->getValue());
Assert::false($semaphore->tryAcquire());
Assert::false($semaphore->acquire(0));

$semaphore->release(0);
Assert::same(0, $semaphore->getValue());
$semaphore->release(3);
Assert::same(3, $semaphore->getValue());

Assert::true($semaphore->tryAcquire());
Assert::true($semaphore->tryAcquire());
Assert::true($semaphore->tryAcquire());
Assert::false($semaphore->tryAcquire());

$semaphore->release(1);
Assert::same(1, $semaphore->getValue());
Assert::true($semaphore->acquire(0));

echo "Done\n";

?>
--EXPECT--
Done
