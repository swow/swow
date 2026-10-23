--TEST--
swow_thread: Futex
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

use Swow\Futex;
use Swow\SyncException;
use Swow\Thread;

Assert::throws(static function (): void {
    new Futex(PHP_INT_MAX);
}, ValueError::class);
Assert::throws(static function (): void {
    Futex::createNamed('', 0);
}, ValueError::class);
Assert::throws(static function (): void {
    Futex::attachNamed('');
}, ValueError::class);
Assert::throws(static function (): void {
    Futex::unlink('');
}, ValueError::class);

$futex = new Futex(1);
Assert::same(1, $futex->getValue());
$futex->setValue(2);
Assert::throws(static function () use ($futex): void {
    $futex->setValue(PHP_INT_MAX);
}, ValueError::class);
Assert::throws(static function () use ($futex): void {
    $futex->wait(PHP_INT_MAX);
}, ValueError::class);
Assert::throws(static function () use ($futex): void {
    $futex->wake(PHP_INT_MAX);
}, ValueError::class);
Assert::throws(static function () use ($futex): void {
    $futex->__unserialize(['id' => 1, 'token' => 1]);
}, SyncException::class);
Assert::same(2, $futex->getValue());
Assert::false($futex->wait(2, 0));
Assert::false($futex->wait(1, 0));
Assert::true($futex->wake(0));
Assert::true($futex->wake(1));
$copy = unserialize(serialize($futex));
Assert::same(2, $copy->getValue());

$futex = new Futex(0);
$thread = Thread::run(static function () use ($futex): void {
    $futex->wait(0);
});
usleep(100000);
$futex->setValue(1);
Assert::true($futex->wake(-1));
$thread->wait();
Assert::same(1, $futex->getValue());

$name = 'ftx-' . getmypid() . '-' . bin2hex(random_bytes(4));
try {
    $named = Futex::createNamed($name, 3);
    $attached = Futex::attachNamed($name);
    $serialized = unserialize(serialize($named));
    Assert::same(3, $serialized->getValue());
    Assert::same(3, $attached->getValue());
    $named->setValue(4);
    Assert::true($attached->wait(4, 0) === false);
    Assert::same(4, $attached->getValue());
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
