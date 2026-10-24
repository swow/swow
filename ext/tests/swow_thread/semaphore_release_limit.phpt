--TEST--
swow_semaphore: a release beyond the platform limit is rejected
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
if (PHP_INT_SIZE < 8) {
    exit('skip needs a 64-bit build');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Semaphore;

$semaphore = new Semaphore(0);
Assert::same(0, $semaphore->getValue());

// uv_sem_post aborts the process when the count passes SEM_VALUE_MAX
Assert::throws(static function () use ($semaphore): void {
    $semaphore->release(2147483648);
}, ValueError::class);
Assert::same(0, $semaphore->getValue());

Assert::throws(static function () use ($semaphore): void {
    $semaphore->release(-1);
}, ValueError::class);
Assert::same(0, $semaphore->getValue());

$semaphore->release(2);
Assert::same(2, $semaphore->getValue());
Assert::true($semaphore->acquire(0));
Assert::true($semaphore->acquire(0));
Assert::false($semaphore->acquire(0));

echo "done\n";
?>
--EXPECT--
done
