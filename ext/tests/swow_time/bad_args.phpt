--TEST--
swow_time: bad arguments passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

set_error_handler(function (...$_) {
    throw new ValueError('converted ValueError');
});

foreach (
    [
        'sleep' => [[-1]],
        'usleep' => [[-1]],
        'msleep' => [[-1]],
        'time_nanosleep' => [
            [-1, -2],
            [0, -2],
            [0, 2000000000],
        ],
        'time_sleep_until' => [[0]],
    ] as $func => $args_set
) {
    foreach ($args_set as $args) {
        Assert::throws(function () use ($func, $args) {
            $func(...$args);
        }, ValueError::class);
    }
}

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
