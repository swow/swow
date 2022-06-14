--TEST--
swow_watchdog: syscall
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
if (PHP_OS_FAMILY !== 'Windows') {
    skip_if_c_function_not_exist('int nanosleep(const void *, void *);');
} else {
    skip_if_c_function_not_exist('void Sleep(uint32_t);', 'kernel32.dll');
}
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\WatchDog;

function ffi_sleep(int $ns): void
{
    if (PHP_OS_FAMILY !== 'Windows') {
        // mocking blocking with nanosleep at unix
        $time_t = 'uint' . (PHP_INT_SIZE * 8) . '_t';
        $ffi = FFI::cdef(
            <<<DEF
typedef struct timespec_t {
    {$time_t} tv_sec;
    long tv_nsec;
} timespec;
int nanosleep(timespec *req, timespec *rem);
DEF
        );
        $ts = $ffi->new('timespec');
        $ts->tv_sec = (int) floor($ns / 1e9);
        $ts->tv_nsec = $ns % (1000 * 1000 * 1000);
        while ($ffi->nanosleep(FFI::addr($ts), FFI::addr($ts)) !== 0) {
            // do nothing
        }
    } else {
        // mocking blocking with Sleep at windows
        $ffi = FFI::cdef(<<<'DEF'
void Sleep(uint32_t);
DEF
            , 'kernel32.dll');
        $ffi->Sleep($ns / (1000 * 1000));
    }
}

switch (PHP_OS_FAMILY) {
    case 'Linux':
        // use default 100us
        $quantum = 100 * 1000;
        break;
    case 'Windows':
        // windows have a 1ms+ timer resolution
        $quantum = 1 * 1000 * 1000; // 1ms
        break;
    case 'Darwin':
    case 'Solaris':
    case 'BSD':
    case 'Unknown':
        // most UNIXs only support a 100hz/10ms timer resolution
        $quantum = 10 * 1000 * 1000; // 10ms
        break;
    default:
        throw new Exception('not supported system');
}
$threshold = 5 * $quantum; // 5 times quantum to bite
$blocking_time = 100 * $quantum; // 100 times quantum

Assert::false(WatchDog::isRunning());

WatchDog::run($quantum, $threshold);

Assert::true(WatchDog::isRunning());

$watcher = Coroutine::run(function () {
    Coroutine::yield();
    WatchDog::stop();
    echo 'I am back' . PHP_LF;
});

// mock blocking
ffi_sleep($blocking_time);

$watcher->resume();

echo 'Done' . PHP_LF;

?>
--EXPECTREGEX--
(?:Warning: <(?:WatchDog)> [\S ]+\n)+I am back
Done
