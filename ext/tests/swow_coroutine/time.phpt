--TEST--
swow_coroutine: time
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

use function Swow\Sync\waitAll;

$coroutine = new Coroutine(static function (): void {
    usleep(100_000);
});
Assert::same($coroutine->getStartTime(), 0);
Assert::same($coroutine->getEndTime(), 0);
$microTime = (int) (microtime(true) * 1000);
$coroutine->resume();
$startTime = $coroutine->getStartTime();
$diff = $microTime > $startTime ? $microTime - $startTime : $startTime - $microTime;
Assert::lessThanEq($diff, 10);
waitAll();
Assert::greaterThan($coroutine->getEndTime(), 0);
Assert::greaterThan($coroutine->getEndTime() - $coroutine->getStartTime(), 10);

echo "Done\n";

?>
--EXPECT--
Done
