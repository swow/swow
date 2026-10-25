--TEST--
swow_thread: Futex error paths
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

use Swow\Futex;
use Swow\SharedBuffer;
use Swow\SyncException;

$constructed = new Futex();
Assert::throws(static function () use ($constructed): void {
    $constructed->__construct();
}, SyncException::class, null, '/already been constructed/');

$id = getmypid() . '-' . bin2hex(random_bytes(4));
$name = 'ftx-' . $id;
$missing = 'missing-' . $id;
$bufferName = 'sbuf-' . $id;

try {
    $named = Futex::createNamed($name, 1);
    Assert::throws(static function () use ($name): void {
        Futex::createNamed($name, 2);
    }, SyncException::class, null, PHP_OS_FAMILY === 'Windows' ? '/already has the name/' : '/shm_open/');
    Assert::throws(static function () use ($missing): void {
        Futex::attachNamed($missing);
    }, SyncException::class);
    if (PHP_OS_FAMILY === 'Windows') {
        /* a named section ends with its last handle, so a missing name is not an error */
        Futex::unlink($missing);
    } else {
        Assert::throws(static function () use ($missing): void {
            Futex::unlink($missing);
        }, SyncException::class);
    }

    # a mapping that holds a SharedBuffer is not a futex
    $buffer = SharedBuffer::createNamed($bufferName, 64);
    Assert::throws(static function () use ($bufferName): void {
        Futex::attachNamed($bufferName);
    }, SyncException::class, null, '/not a futex/');

    # a positive timeout reaches the waiting syscall and reports a timeout
    $waiter = new Futex(0);
    $started = microtime(true);
    Assert::false($waiter->wait(0, 30));
    Assert::true(microtime(true) - $started >= 0.02);
    Assert::false($waiter->wait(0, 0));
    Assert::false($waiter->wait(1, 0));

    # unserializing into a constructed object drops the old handle
    $source = new Futex(7);
    $target = new Futex(0);
    $target->__unserialize($source->__serialize());
    Assert::same(7, $target->getValue());
    $target->__unserialize(['name' => $name]);
    Assert::same(1, $target->getValue());
    Assert::throws(static function () use ($target): void {
        $target->__unserialize(['name' => 1]);
    }, SyncException::class);
    Assert::throws(static function () use ($target, $missing): void {
        $target->__unserialize(['name' => $missing]);
    }, SyncException::class);

    Assert::same(['name' => $name, 'value' => 1], $named->__debugInfo());
    Assert::same(['value' => 7], $source->__debugInfo());
} finally {
    try {
        Futex::unlink($name);
    } catch (Throwable) {
    }
    try {
        SharedBuffer::unlink($bufferName);
    } catch (Throwable) {
    }
}

echo "Done\n";
?>
--EXPECT--
Done
