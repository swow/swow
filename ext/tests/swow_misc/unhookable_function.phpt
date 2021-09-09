--TEST--
swow_misc: hook unhookable function
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
disable_functions=dummyOne, msleep,dummy_2, usleep
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
set_error_handler(function (...$_) {
    echo "caught a PHP7 warning\n";
});

try {
    msleep(1);
} catch (Error $e) {
    echo "caught a PHP8 exception\n";
}

try {
    usleep(1);
} catch (Error $e) {
    echo "caught a PHP8 exception\n";
}

echo 'Done' . PHP_LF;
?>
--EXPECTREGEX--
caught a (PHP7 warning|PHP8 exception)
caught a (PHP7 warning|PHP8 exception)
Done
