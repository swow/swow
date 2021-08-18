--TEST--
swow_time: sleep zero
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

foreach (
    [
        ['sleep', [0], 0],
        ['usleep', [0], null],
        ['msleep', [0], 0],
        ['time_nanosleep', [0, 0], true],
    ] as $func
) {
    Assert::same($func[0](...$func[1]), $func[2]);
}

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
