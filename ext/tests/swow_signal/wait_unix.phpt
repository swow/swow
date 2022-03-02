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
use Swow\SignalException;

Coroutine::run(function () {
    Signal::wait(Signal::INT, 1000);
    echo "wait OK\n";
});
Signal::kill(getmypid(), Signal::INT);

Assert::throws(function () {
    // timeout situation
    Signal::wait(Signal::TERM, 10);
}, SignalException::class);

echo 'Done' . PHP_LF;
?>
--EXPECTREGEX--
wait OK
Done

