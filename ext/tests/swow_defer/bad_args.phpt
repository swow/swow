--TEST--
swow_defer: bad arguments passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Defer;

set_error_handler(function ($errno, $errstr, $errfile, $errline) {
    throw new Error($errstr, $errno);
}, E_WARNING);

Assert::throws(function () {
    // argument must be a callable
    Defer('I am not a callback');
}, Error::class);

Assert::throws(function () {
    // cannot be called dynamically
    call_user_func(Defer::class, 'NULL');
}, Error::class);

Assert::throws(function () {
    // cannot be constructed
    new Swow\Defer();
}, Error::class);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
