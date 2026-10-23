--TEST--
swow_thread: ThreadBarrier
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

use Swow\Sync\ThreadBarrier;
use Swow\SyncException;
use Swow\Thread;

Assert::throws(static function (): void {
    new ThreadBarrier(0);
}, ValueError::class);

$single = new ThreadBarrier(1);
Assert::same(1, $single->getCount());
Assert::same(0, $single->getWaiting());
Assert::true($single->wait());

$barrier = new ThreadBarrier(2);
$thread = Thread::run(static function () use ($barrier): void {
    for ($i = 0; $i < 3; $i++) {
        $barrier->wait();
    }
});

for ($i = 0; $i < 3; $i++) {
    Assert::true($barrier->wait());
}
$thread->wait();

$barrier = new ThreadBarrier(2);
Assert::false($barrier->wait(0));
Assert::same(0, $barrier->getWaiting());

$barrier = new ThreadBarrier(2);
$invalid = new ThreadBarrier(2);
Assert::throws(static function () use ($invalid): void {
    $invalid->__unserialize(['id' => 1, 'token' => 1]);
}, SyncException::class);
$copy = unserialize(serialize($barrier));
Assert::same(2, $copy->getCount());
Assert::same(0, $copy->getWaiting());
$debug = $copy->__debugInfo();
Assert::same(2, $debug['count']);

echo "Done\n";

?>
--EXPECT--
Done
