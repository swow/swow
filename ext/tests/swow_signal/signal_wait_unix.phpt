--TEST--
swow_signal: Swow\Signal::wait static method for unix
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Signal;

$posix_kill = 'posix_kill';

if (is_callable($posix_kill)) {
    Coroutine::run(function () {
        Signal::wait(Signal::INT, 1000);
        echo "wait OK\n";
    });
    $posix_kill(getmypid(), Signal::INT);
} else {
    echo "wait SKIP\n";
}

Assert::throws(function () {
    // timeout situation
    Signal::wait(Signal::TERM, 10);
}, \Swow\Signal\Exception::class);

echo 'Done' . PHP_LF;
?>
--EXPECTREGEX--
wait (OK|SKIP)
Done

