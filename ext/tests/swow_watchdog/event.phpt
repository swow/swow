--TEST--
swow_watchdog: structured events, episodes, backoff and recent events
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_in_valgrind();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;
use Swow\Watchdog;
use Swow\WatchdogException;

Assert::same(Watchdog::getRecentEvents(), []);
Assert::throws(static fn() => Watchdog::run(0, 0, null, ['unknown' => 1]), WatchdogException::class);
Assert::throws(static fn() => Watchdog::run(0, 0, null, ['backoffMax' => -1]), WatchdogException::class);

$events = [];
$quantum = 10 * 1000 * 1000;
/* a large threshold keeps a slow interrupt from being taken for a syscall block */
Watchdog::run($quantum, 1000 * $quantum, static function (string $type, array $event) use (&$events): void {
    $events[] = $event;
}, ['backoffBase' => 2, 'backoffMax' => 8, 'recentEvents' => 3]);

$wr = new WaitReference();
$burner = Coroutine::run(static function () use ($wr, &$events): void {
    /* burn until three alerts arrived, slow CI machines need more than a few quanta */
    $start = microtime(true);
    while (count($events) < 3 && microtime(true) - $start < 10) {
    }
});
Coroutine::run(static function () use ($wr): void {
    usleep(1000);
});
WaitReference::wait($wr);
Watchdog::stop();

Assert::greaterThanEq(count($events), 3);
$first = $events[0];
Assert::same($first['type'], 'cpu');
Assert::same($first['delivery'], 'immediate');
Assert::same($first['coroutineId'], $burner->getId());
Assert::same($first['quantumNs'], $quantum);
Assert::greaterThan($first['blockedNs'], 0);
Assert::null($first['lastKnownLocation']);
Assert::isList($first['trace']);
/* frames are debug_backtrace() frames without arguments; the top frame may
 * be the coroutine closure itself, which has no caller line */
Assert::keyExists($first['trace'][0], 'function');
Assert::keyNotExists($first['trace'][0], 'args');
Assert::same(json_decode(json_encode($first), true), $first);

/* backoff switches the coroutine, but its alerts stay in one episode */
$backoffs = [];
foreach ($events as $index => $event) {
    Assert::same($event['seq'], $index + 1);
    Assert::same($event['episode']['id'], $first['episode']['id']);
    Assert::same($event['episode']['count'], $index + 1);
    $backoffs[] = intdiv($event['backoffNs'], 1000 * 1000);
}
Assert::same(array_slice($backoffs, 0, 3), [2, 4, 8]);
Assert::same(max($backoffs), 8);

echo "Done\n";
?>
--EXPECT--
Done
