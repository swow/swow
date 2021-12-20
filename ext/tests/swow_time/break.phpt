--TEST--
swow_time: break sleep
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

const LONG_TIME = 5;

foreach ([
    'sleep' => LONG_TIME,
    'msleep' => LONG_TIME * 1000,
] as $func => $arg) {
    $co = Coroutine::run(function () use ($func, $arg) {
        echo "start {$func}({$arg})\n";
        $ret = $func($arg);
        // fixme: KNOWN BC here
        // original will return 192 on Windows
        Assert::greaterThan($ret, 0);
    });
    $co->resume();
}

$start = microtime(true);
$co = Coroutine::run(function () {
    $usleep_time = LONG_TIME * 1000 * 1000;
    echo "start usleep({$usleep_time})\n";
    usleep($usleep_time);
});
$co->resume();
$end = microtime(true);
Assert::lessThan($end - $start, LONG_TIME * 1000);

$start = microtime(true);
$co = Coroutine::run(function () {
    echo 'start time_nanosleep(' . LONG_TIME . ", 0)\n";
    $ret = time_nanosleep(LONG_TIME, 0);
    Assert::integer($ret['seconds']);
    Assert::integer($ret['nanoseconds']);
    $remaining = $ret['seconds'] * 1000 * 1000 * 1000 + $ret['nanoseconds'];
    Assert::greaterThan($remaining, 0);
});
$co->resume();
$end = microtime(true);
Assert::lessThan($end - $start, LONG_TIME * 1000);

$start = microtime(true);
$co = Coroutine::run(function () use ($start) {
    $sleep_ms = LONG_TIME * 1000;
    echo "start time_sleep_until(now + {$sleep_ms})\n";
    $ret = time_sleep_until($start + $sleep_ms);
    Assert::false($ret);
});
$co->resume();
$end = microtime(true);
Assert::lessThan($end - $start, LONG_TIME * 1000);

echo 'Done' . PHP_LF;
?>
--EXPECT--
start sleep(5)
start msleep(5000)
start usleep(5000000)
start time_nanosleep(5, 0)
start time_sleep_until(now + 5000)
Done
