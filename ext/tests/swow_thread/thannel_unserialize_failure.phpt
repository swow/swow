--TEST--
swow_thannel: a batch that fails to unserialize leaves the return value undefined
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

use Swow\Thannel;

class SwowTestWakeupBoom
{
    public function __wakeup(): void
    {
        throw new Exception('boom');
    }
}

function fillHeap(): void
{
    for ($i = 0; $i < 20000; $i++) {
        $chunks[] = str_repeat('x', 64);
    }
}

$thannel = new Thannel(4);
$thannel->push('keep1');
$thannel->push(new SwowTestWakeupBoom());
$thannel->push('keep2');

// the failing path drops the collected array once, and the engine must not drop
// it a second time
Assert::throws(static function () use ($thannel): void {
    $thannel->popMany(10, 0);
}, Throwable::class);
fillHeap();

$thannel = new Thannel(4);
$thannel->push('keep1');
$thannel->push(new SwowTestWakeupBoom());
$thannel->push('keep2');
Assert::throws(static function () use ($thannel): void {
    $thannel->drain();
}, Throwable::class);
fillHeap();

echo "heap ok\n";
?>
--EXPECT--
heap ok
