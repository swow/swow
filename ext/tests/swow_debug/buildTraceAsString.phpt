--TEST--
swow_debug: buildTraceAsString
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\Debug\buildTraceAsString;

function call(callable $callable)
{
    return $callable();
}

class A
{
    public static self $instance;

    /* @var $callable */
    private $callable;

    private Generator $generator;

    private function __construct(callable $callable)
    {
        $this->callable = $callable;
        $this->generator = $this->generator();
    }

    public function __invoke()
    {
        return call($this->callable);
    }

    private function generator()
    {
        yield 1;
        var_dump(buildTraceAsString(debug_backtrace()));

        return 2;
    }

    private function call()
    {
        return $this();
    }

    public static function callStatic(callable $callable)
    {
        array_map(function ($e) use ($callable) {
            if (!isset(self::$instance)) {
                self::$instance = new self($callable);
            }
            self::$instance->generator->next();

            return self::$instance->call();
        }, [-1]);
    }
}

class MyWarningException extends ErrorException
{
}
set_error_handler(function ($errno, $errstr, $errfile, $errline) {
    var_dump(buildTraceAsString(debug_backtrace()));
    throw new MyWarningException($errstr, $errno, 1, $errfile, $errline);
}, E_WARNING);

// frame should be an array from debug_backtrace
Assert::throws(function () {
    buildTraceAsString(['im a bad frame']);
}, MyWarningException::class);

(function () {
    A::callStatic(function () {
        var_dump(buildTraceAsString(debug_backtrace()));

        return 1;
    });
})();

var_dump(buildTraceAsString(debug_backtrace()));

echo 'Done' . PHP_LF;
?>
--EXPECTF--
string(%d) "#0 [internal function]: {closure}(%s)
#1 %sbuildTraceAsString.php(%d): Swow\Debug\buildTraceAsString(Array)
#2 %s(%d): {closure}()
#3 %sbuildTraceAsString.php(%d): Assert::throws(Object(Closure), 'MyWarningExcept...')
#4 {main}"
string(%d) "#0 [internal function]: A->generator()
#1 %sbuildTraceAsString.php(%d): Generator->next()
#2 [internal function]: A::{closure}(-1)
#3 %sbuildTraceAsString.php(%d): array_map(Object(Closure), Array)
#4 %sbuildTraceAsString.php(%d): A::callStatic(Object(Closure))
#5 %sbuildTraceAsString.php(%d): {closure}()
#6 {main}"
string(%d) "#0 %sbuildTraceAsString.php(%d): {closure}()
#1 %sbuildTraceAsString.php(%d): call(Object(Closure))
#2 %sbuildTraceAsString.php(%d): A->__invoke()
#3 %sbuildTraceAsString.php(%d): A->call()
#4 [internal function]: A::{closure}(-1)
#5 %sbuildTraceAsString.php(%d): array_map(Object(Closure), Array)
#6 %sbuildTraceAsString.php(%d): A::callStatic(Object(Closure))
#7 %sbuildTraceAsString.php(%d): {closure}()
#8 {main}"
string(9) "#0 {main}"
Done
