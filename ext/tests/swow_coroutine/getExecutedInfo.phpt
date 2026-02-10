--TEST--
swow_coroutine: get executed info
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

spl_autoload_register(static function (string $class): void {
    var_dump($class);
    echo "\n";

    class TestGetExecutedInfo
    {
        public static function doNothing(): void
        {
        }
    }

    echo Coroutine::getCurrent()->getTraceAsString(), "\n\n";

    var_dump(Coroutine::getCurrent()->getDefinedVars(0)['class']);
    var_dump(Coroutine::getCurrent()->getDefinedVars(1));
    var_dump(Coroutine::getCurrent()->getDefinedVars(2)['tip']);
    var_dump(Coroutine::getCurrent()->getDefinedVars(3)['tip']);
    var_dump(Coroutine::getCurrent()->getDefinedVars(4)['tip']);
    var_dump(Coroutine::getCurrent()->getDefinedVars(5)['tip']);
    echo "\n";

    $depth = Coroutine::getCurrent()->getTraceDepth();
    for ($n = 0; $n < $depth; $n++) {
        echo sprintf(
            "#%d %s(%d): %s()\n",
            $n,
            Coroutine::getCurrent()->getExecutedFilename($n + 1),
            Coroutine::getCurrent()->getExecutedLineno($n + 1),
            Coroutine::getCurrent()->getExecutedFunctionName($n)
        );
    }
    echo "\n";
});

class TestGetExecutedInfoFunctions
{
    public static function a(): void
    {
        $tip = 'I am ' . strtoupper(__FUNCTION__);
        static::b();
    }

    public static function b(): void
    {
        $tip = 'I am ' . strtoupper(__FUNCTION__);
        static::c();
    }

    public static function c(): void
    {
        $tip = 'I am ' . strtoupper(__FUNCTION__);
        require __DIR__ . '/getExecutedInfo.inc';
    }
}

TestGetExecutedInfoFunctions::a();

echo "Done\n";
?>
--EXPECTF--
string(19) "TestGetExecutedInfo"

#0 %sgetExecutedInfo.php(%d): Swow\Coroutine->getTraceAsString()
#1 %sgetExecutedInfo.inc(%d): {closur%s}('TestGetExecuted...')
#2 %sgetExecutedInfo.php(%d): require('%s...')
#3 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::c()
#4 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::b()
#5 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::a()
#6 {main}

string(19) "TestGetExecutedInfo"
array(0) {
}
string(6) "I am C"
string(6) "I am C"
string(6) "I am B"
string(6) "I am A"

#0 %sgetExecutedInfo.php(%d): Swow\Coroutine::getExecutedFunctionName()
#1 %sgetExecutedInfo.inc(%d): {closur%s}()
#2 %sgetExecutedInfo.php(%d): require()
#3 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::c()
#4 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::b()
#5 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::a()

Done
