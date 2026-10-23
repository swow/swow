--TEST--
swow_thread: Thannel batch API
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
use Swow\ThannelException;

$thannel = new Thannel(3);
Assert::same(0, $thannel->pushMany([]));
Assert::same([], $thannel->drain());

$thannel->pushMany([1, 2, 3]);
Assert::same(3, $thannel->getLength());
Assert::same([1, 2], $thannel->popMany(2));
Assert::same([3], $thannel->drain());
Assert::true($thannel->isEmpty());

Assert::throws(static function () use ($thannel): void {
    $thannel->pushMany([1, 2, 3, 4], 0);
}, ThannelException::class);
Assert::throws(static function () use ($thannel): void {
    $thannel->popMany(0, 0);
}, ValueError::class);
Assert::throws(static function () use ($thannel): void {
    $thannel->popMany(1, 0);
}, ThannelException::class);

$thannel->push('pending');
$thannel->close();
Assert::same('pending', $thannel->pop(0));
Assert::same([], $thannel->drain());
Assert::throws(static function () use ($thannel): void {
    $thannel->pop(0);
}, ThannelException::class);
$thannel->close();

Assert::true($thannel->getWakeCount() >= 0);
Assert::true($thannel->getParkCount() >= 0);

echo "Done\n";

?>
--EXPECT--
Done
