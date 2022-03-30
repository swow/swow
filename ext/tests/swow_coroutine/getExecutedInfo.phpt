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

spl_autoload_register(function (string $class) {
    Assert::same($class, TestGetExecutedInfo::class);

    class TestGetExecutedInfo
    {
        public static function doNothing(): void { }
    }

    echo Coroutine::getCurrent()->getTraceAsString(), "\n\n";

    Assert::same(Coroutine::getCurrent()->getDefinedVars(0)['class'], TestGetExecutedInfo::class);
    Assert::isEmpty(Coroutine::getCurrent()->getDefinedVars(1));
    Assert::same(Coroutine::getCurrent()->getDefinedVars(2)['tip'], 'I am C');
    Assert::same(Coroutine::getCurrent()->getDefinedVars(3)['tip'], 'I am C');
    Assert::same(Coroutine::getCurrent()->getDefinedVars(4)['tip'], 'I am B');
    Assert::same(Coroutine::getCurrent()->getDefinedVars(5)['tip'], 'I am A');

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
    public static function a()
    {
        $tip = 'I am ' . strtoupper(__FUNCTION__);
        static::b();
    }

    public static function b()
    {
        $tip = 'I am ' . strtoupper(__FUNCTION__);
        static::c();
    }

    public static function c()
    {
        $tip = 'I am ' . strtoupper(__FUNCTION__);
        require __DIR__ . '/getExecutedInfo.inc';
    }
}

TestGetExecutedInfoFunctions::a();

echo 'Done' . PHP_LF;
?>
--EXPECTF--

#0 %sgetExecutedInfo.php(%d): Swow\Coroutine->getTraceAsString()
#1 %sgetExecutedInfo.inc(%d): {closure}('TestGetExecuted...')
#2 %sgetExecutedInfo.php(%d): require('%s...')
#3 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::c()
#4 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::b()
#5 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::a()
#6 {main}

#0 %sgetExecutedInfo.php(%d): Swow\Coroutine::getExecutedFunctionName()
#1 %sgetExecutedInfo.inc(%d): {closure}()
#2 %sgetExecutedInfo.php(%d): require()
#3 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::c()
#4 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::b()
#5 %sgetExecutedInfo.php(%d): TestGetExecutedInfoFunctions::a()

Done
