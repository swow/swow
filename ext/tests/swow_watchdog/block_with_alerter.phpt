--TEST--
swow_watchdog: Debug\block() with alerter callback
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Debug;
use Swow\Sync\WaitReference;
use Swow\Watchdog;

switch (PHP_OS_FAMILY) {
    case 'Linux':
        $quantum = 10 * 1000 * 1000; // 10ms - conservative to avoid false positives
        break;
    case 'Windows':
        $quantum = 10 * 1000 * 1000; // 10ms
        break;
    case 'Darwin':
    case 'Solaris':
    case 'BSD':
    case 'Unknown':
        $quantum = 10 * 1000 * 1000; // 10ms
        break;
    default:
        throw new Exception('not supported system');
}
$threshold = 10 * $quantum; // 100ms - large enough to avoid false positives
$blocking_time_ms = (int) (30 * $quantum / (1000 * 1000)); // 300ms blocking

$cpu_alerted = false;
$syscall_alerted = false;

Watchdog::run(
    quantum: $quantum,
    threshold: $threshold,
    alerter: static function (string $blockingType) use (&$cpu_alerted, &$syscall_alerted): void {
        if ($blockingType === 'cpu') {
            $cpu_alerted = true;
            echo "CPU blocking detected\n";
            sleep(0); // yield
        } elseif ($blockingType === 'syscall') {
            $syscall_alerted = true;
            echo "Syscall blocking detected\n";
        }
    }
);

// Test 1: CPU blocking
$wr = new WaitReference();
Coroutine::run(static function () use ($threshold, $wr): void {
    $start = microtime(true);
    while (microtime(true) - $start < ($threshold / 1e9) * 2) {
        // busy loop to trigger CPU blocking
    }
});
WaitReference::wait($wr);

Assert::true($cpu_alerted, 'CPU blocking should trigger alerter');

// Test 2: Syscall blocking with Debug\block()
$wr = new WaitReference();
Coroutine::run(static function () use ($blocking_time_ms, $wr): void {
    Debug\block($blocking_time_ms);
});
WaitReference::wait($wr);

Assert::true($syscall_alerted, 'Syscall blocking should trigger alerter');

Watchdog::stop();

echo "Done\n";

?>
--EXPECTREGEX--
(?:Warning:[^\n]*\n)?CPU blocking detected
[\s\S]*?Warning: <Watchdog>[\s\S]*?Syscall blocking detected
Done
