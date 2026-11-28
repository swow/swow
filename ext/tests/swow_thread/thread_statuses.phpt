--TEST--
swow_thread: another thread reports the watchdog status of a blocked thread
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
skip_if_in_valgrind();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Thread;

use function Swow\Debug\getThreadStatuses;

$statuses = getThreadStatuses();
Assert::count($statuses, 1);
Assert::true($statuses[0]['isMain']);
Assert::true($statuses[0]['isCurrent']);
Assert::null($statuses[0]['blocking']);

/* the thread blocks inside a coroutine, so its own loop can not answer */
$thread = Thread::runCode(<<<'CODE'
    Swow\Watchdog::run(10 * 1000 * 1000, 50 * 1000 * 1000);
    Swow\Coroutine::run(static function (): void {
        Swow\Debug\block(600);
    });
    usleep(1000);
    Swow\Watchdog::stop();
    CODE);

$blocking = null;
$start = microtime(true);
while ($blocking === null && microtime(true) - $start < 2) {
    usleep(20 * 1000);
    foreach (getThreadStatuses() as $status) {
        if (!$status['isMain'] && $status['blocking'] !== null && $status['blocking']['durationNs'] > 100 * 1000 * 1000) {
            $blocking = $status;
        }
    }
}
Assert::notNull($blocking);
Assert::false($blocking['isCurrent']);
Assert::true($blocking['watchdog']['running']);
Assert::same($blocking['watchdog']['quantumNs'], 10 * 1000 * 1000);
Assert::same($blocking['blocking']['type'], 'syscall');
Assert::greaterThan($blocking['blocking']['coroutineId'], 1);

$thread->wait();
Assert::count(getThreadStatuses(), 1);

echo "Done\n";
?>
--EXPECTREGEX--
(?:Warning: <Watchdog>[^\n]*\n(?: +[^\n]*\n)*)*Done
