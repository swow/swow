--TEST--
swow_watch_dog: syscall
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_extension_not_exist('ffi');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\WatchDog;

// 0.1ms quantum + 1ms threshold
WatchDog::run(100 * 1000, 1 * 1000 * 1000);

Coroutine::run(function () {
    sleep(0);
    echo 'I am back' . PHP_LF;
});

$ffi = FFI::cdef(<<<C
int usleep(unsigned int usec);
C
);
// syscall blocking 100ms
$ffi->usleep(100 * 1000 * (PHP_OS_FAMILY === 'Darwin' ? 10 : 1));

?>
--EXPECTREGEX--
(?:Warning: <(?:Watch-Dog)> [\S ]+\n)+I am back
