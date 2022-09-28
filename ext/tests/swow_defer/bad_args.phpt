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

set_error_handler(static function ($errno, $errstr, $errfile, $errline): void {
    throw new Error($errstr, $errno);
}, E_WARNING);

Assert::throws(static function (): void {
    // argument must be a callable
    Defer('I am not a callback');
}, Error::class);

Assert::throws(static function (): void {
    // cannot be called dynamically
    call_user_func(Defer::class, 'NULL');
}, Error::class);

Assert::throws(static function (): void {
    // cannot be constructed
    new Swow\Defer();
}, Error::class);

echo "Done\n";
?>
--EXPECT--
Done
