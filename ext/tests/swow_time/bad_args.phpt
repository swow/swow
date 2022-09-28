--TEST--
swow_time: bad arguments passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

set_error_handler(static function (...$_): void {
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
        Assert::throws(static function () use ($func, $args): void {
            $func(...$args);
        }, ValueError::class);
    }
}

echo "Done\n";
?>
--EXPECT--
Done
