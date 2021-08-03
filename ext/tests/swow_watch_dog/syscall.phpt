--TEST--
swow_watch_dog: syscall
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

// should be 0.1ms quantum + 1ms threshold
if (PHP_OS_FAMILY != 'Windows') {
    WatchDog::run(100 * 1000, 1 * 1000 * 1000);
} else {
    // but at windows, sleep functions only accept 1ms+
    WatchDog::run(1 * 1000 * 1000, 5 * 1000 * 1000);
}

Coroutine::run(function () {
    sleep(0);
    WatchDog::stop();
    echo 'I am back' . PHP_LF;
});

if (PHP_OS_FAMILY != 'Windows') {
    // mocking blocking with nanosleep at unix
    $time_t = 'uint' . (string)(PHP_INT_SIZE * 8) . '_t';
    $ffi = FFI::cdef(<<<DEF
typedef struct timespec_t {
    $time_t tv_sec;
    long tv_nsec;
} timespec;
int nanosleep(timespec *req, timespec *rem);
DEF
    );
    $ts = $ffi->new('timespec');
    // syscall blocking 100ms
    $ts->tv_sec = 0;
    $ts->tv_nsec = 100 * 1000 * 1000;
    while ($ffi->nanosleep(\FFI::addr($ts), \FFI::addr($ts)) != 0) {
        // do nothing
    }
} else {
    // mocking blocking with Sleep at windows
    $ffi = FFI::cdef(<<<DEF
void Sleep(uint32_t);
DEF
    , 'kernel32.dll');
    // syscall blocking 100ms
    $ffi->Sleep(100);
}

?>
--EXPECTREGEX--
(?:Warning: <(?:Watch-Dog)> [\S ]+\n)+I am back
