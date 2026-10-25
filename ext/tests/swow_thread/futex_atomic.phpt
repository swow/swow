--TEST--
swow_thread: Futex atomic operations
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_class_not_exist(Swow\Futex::class);
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Futex;

$futex = new Futex(10);
Assert::same($futex->getValue(), 10);

Assert::same($futex->add(5), 10);
Assert::same($futex->getValue(), 15);
Assert::same($futex->add(-1), 15);
Assert::same($futex->getValue(), 14);
Assert::same($futex->exchange(-3), 14);
Assert::same($futex->getValue(), -3);

# add wraps at both ends of the 32-bit range
$futex->setValue(2147483647);
Assert::same($futex->add(1), 2147483647);
Assert::same($futex->getValue(), -2147483648);
$futex->setValue(-2147483648);
Assert::same($futex->add(-1), -2147483648);
Assert::same($futex->getValue(), 2147483647);

# a failed compare-exchange leaves the value alone
$futex->setValue(7);
Assert::true($futex->compareExchange(7, 8));
Assert::same($futex->getValue(), 8);
Assert::false($futex->compareExchange(7, 9));
Assert::same($futex->getValue(), 8);

Assert::throws(static function () use ($futex): void {
    $futex->add(PHP_INT_MAX);
}, ValueError::class);
Assert::throws(static function () use ($futex): void {
    $futex->exchange(PHP_INT_MAX);
}, ValueError::class);
Assert::throws(static function () use ($futex): void {
    $futex->compareExchange(PHP_INT_MAX, 0);
}, ValueError::class);
Assert::throws(static function () use ($futex): void {
    $futex->compareExchange(0, PHP_INT_MAX);
}, ValueError::class);
Assert::same($futex->getValue(), 8);

# the atomic operations work on the shared mapping
$name = 'ftx-' . getmypid() . '-' . bin2hex(random_bytes(4));
try {
    $named = Futex::createNamed($name, 1);
    $attached = Futex::attachNamed($name);
    Assert::same($named->add(2), 1);
    Assert::same($attached->getValue(), 3);
    Assert::true($attached->compareExchange(3, 4));
    Assert::same($named->getValue(), 4);
    Assert::same($attached->exchange(5), 4);
    Assert::same($named->getValue(), 5);
} finally {
    try {
        Futex::unlink($name);
    } catch (Throwable) {
    }
}

echo "Done\n";

?>
--EXPECT--
Done
