--TEST--
swow_debug: event loop metrics and delay monitor
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\Debug\getCoroutineSummaries;
use function Swow\Debug\getEventLoopDelay;
use function Swow\Debug\getEventLoopMetrics;
use function Swow\Debug\startEventLoopDelayMonitor;
use function Swow\Debug\stopEventLoopDelayMonitor;

usleep(1000);
$metrics = getEventLoopMetrics();
foreach (['round', 'loopCount', 'events', 'eventsWaiting', 'idleTimeNs', 'activeHandles', 'activeRequests'] as $key) {
    Assert::integer($metrics[$key]);
}
Assert::greaterThan($metrics['loopCount'], 0);
Assert::isArray($metrics['handlesByType']);

startEventLoopDelayMonitor(5);
Assert::throws(static fn() => startEventLoopDelayMonitor(5), Error::class);
usleep(20 * 1000);
/* hold the loop for 50ms, the timer fires late by about that much */
$start = microtime(true);
while (microtime(true) - $start < 0.05) {
}
usleep(20 * 1000);
$delay = getEventLoopDelay(true);
Assert::same($delay['resolutionNs'], 5 * 1000 * 1000);
Assert::greaterThan($delay['count'], 0);
Assert::greaterThanEq($delay['maxNs'], 30 * 1000 * 1000);
Assert::greaterThanEq($delay['p99Ns'], $delay['p50Ns']);
Assert::same(getEventLoopDelay()['maxNs'] < $delay['maxNs'], true);
stopEventLoopDelayMonitor();
Assert::throws(static fn() => getEventLoopDelay(), Error::class);

$summaries = getCoroutineSummaries();
Assert::same(count($summaries), Swow\Coroutine::count());
Assert::same(array_keys($summaries[0]), ['id', 'role', 'state', 'parentId', 'label', 'switches', 'startedAt', 'elapsedMs', 'busyNs', 'createdBy', 'wait']);

echo "Done\n";
?>
--EXPECT--
Done
