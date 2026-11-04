--TEST--
swow_watchdog: reports the execution location captured on the watched thread
--EXTENSIONS--
ffi
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
/* the VM interrupt does not reliably reach the watched thread there, which
 * leaves the capture empty and the warning without a location to report */
skip_if(PHP_OS_FAMILY === 'Darwin', 'the interrupt timing makes the capture unreliable');
if (PHP_OS_FAMILY !== 'Windows') {
    skip_if_c_function_not_exist('int usleep(unsigned int usec);');
} else {
    skip_if_c_function_not_exist('void Sleep(uint32_t);', 'kernel32.dll');
}
skip_if_in_valgrind();
?>
--INI--
memory_limit=256M
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Watchdog;

/*
 * The location is captured when the watched thread reaches a VM interrupt, so
 * the coroutine runs interruptible code first and only then blocks inside C.
 * The blocking call keeps the thread out of the VM, which makes the watchdog
 * report the captured frame instead of an empty snapshot.
 *
 * An alerter is installed so the watchdog does not park the coroutine at the
 * interrupt point: the coroutine has to reach the C call for this test.
 */

function block_inside_c(float $seconds): void
{
    if (PHP_OS_FAMILY !== 'Windows') {
        $ffi = FFI::cdef('int usleep(unsigned int usec);');
        $ffi->usleep((int) ($seconds * 1000000));
    } else {
        $ffi = FFI::cdef('void Sleep(uint32_t);', 'kernel32.dll');
        $ffi->Sleep((int) ($seconds * 1000));
    }
}

final class LocationProbe
{
    public function run(float $seconds): void
    {
        // let the VM deliver an interrupt so this frame becomes the snapshot
        $start = microtime(true);
        while (microtime(true) - $start < 0.05) {
        }
        block_inside_c($seconds);
    }
}

$quantum = 10 * 1000 * 1000; // 10ms
Watchdog::run($quantum, 3 * $quantum, static function (string $blockingType): void {
});

Coroutine::run(static function (): void {
    (new LocationProbe())->run(1.0);
});

Watchdog::stop();

echo "Done\n";

?>
--EXPECTF--
Warning: <Watchdog> Syscall blocking or CPU starvation may occur in %s %d, it has been blocked for more than %d ns
         Coroutine: #%d (total: %d)
         Location: LocationProbe::run() at %s
%aDone
