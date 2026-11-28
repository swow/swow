--TEST--
swow_coroutine: parent id, creation site, label inheritance, wait and busy time
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

$main = Coroutine::getCurrent();
Assert::same($main->getParentId(), 0);
Assert::null($main->getWait());

$main->setLabel('request');
$channel = new Channel();
$popper = Coroutine::run(static function () use ($channel): void {
    $channel->pop();
});
$line = __LINE__ - 3;
Assert::same($popper->getParentId(), $main->getId());
Assert::same($popper->getLabel(), 'request');
$location = $popper->getCreatedLocation();
Assert::same($location['file'], __FILE__);
Assert::same($location['line'], $line);
Assert::null($location['function']);

$wait = $popper->getWait();
Assert::same($wait['kind'], 'channel');
Assert::same($wait['op'], 'pop');
Assert::same($wait['resourceId'], 'channel:' . $channel->getId());
Assert::null($wait['timeoutNs']);
Assert::greaterThanEq($wait['durationNs'], 0);

$sleeper = Coroutine::run(static function (): void {
    usleep(100 * 1000);
});
$wait = $sleeper->getWait();
Assert::same($wait['kind'], 'sleep');
Assert::same($wait['timeoutNs'], 100 * 1000 * 1000);
Assert::null($wait['object']);

$yielder = Coroutine::run(static function (): void {
    Coroutine::yield();
});
Assert::same($yielder->getWait()['kind'], 'none');

/* a label set on the child stays on the child */
$popper->setLabel(null);
Assert::null($popper->getLabel());
Assert::same($main->getLabel(), 'request');

Assert::null($main->getBusyTime());
Assert::false(Coroutine::setBusyTimeAccounting(true));
$start = microtime(true);
while (microtime(true) - $start < 0.02) {
}
Assert::greaterThanEq($main->getBusyTime(), 10 * 1000 * 1000);
Assert::true(Coroutine::setBusyTimeAccounting(false));

$channel->push(1);
$sleeper->kill();
$yielder->resume();

echo "Done\n";
?>
--EXPECT--
Done
