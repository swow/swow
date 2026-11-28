--TEST--
swow_watchdog: a syscall event carries the blocking call on top of its trace
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_OS === 'Darwin', 'watchdog syscall detection is unreliable on macOS');
skip_if_in_valgrind();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Debug;
use Swow\Sync\WaitReference;
use Swow\Watchdog;

$quantum = 10 * 1000 * 1000;
$events = [];
Watchdog::run($quantum, 10 * $quantum, static function (string $type, array $event) use (&$events): void {
    $events[] = $event;
});

$wr = new WaitReference();
$blocker = Coroutine::run(static function () use ($wr): void {
    Debug\block(300);
});
WaitReference::wait($wr);

$recent = Watchdog::getRecentEvents();
Watchdog::stop();

$syscall = null;
foreach ($events as $event) {
    if ($event['type'] === 'syscall') {
        $syscall = $event;
    }
}
Assert::notNull($syscall);
Assert::same($syscall['delivery'], 'deferred');
Assert::same($syscall['coroutineId'], $blocker->getId());
Assert::greaterThanEq($syscall['blockedNs'], 200 * 1000 * 1000);
/* since PHP 8.4 (php-src #14627) the VM checks interrupts while the internal
 * frame is still on the stack, so the blocking call itself is on top; before
 * that the frame is already popped and the top frame is its caller */
Assert::same($syscall['trace'][0]['function'], PHP_VERSION_ID >= 80400 ? 'Swow\Debug\block' : '{closure}');
Assert::same($recent[count($recent) - 1]['seq'], $syscall['seq']);

echo "Done\n";
?>
--EXPECTREGEX--
(?:Warning: <Watchdog>[^\n]*\n(?: +[^\n]*\n)*)*Done
