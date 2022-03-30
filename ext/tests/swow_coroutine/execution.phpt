--TEST--
swow_coroutine: execution
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

class TestGetExecution
{
    public static function test(): void
    {
        $tip = 'I am ' . strtoupper(__FUNCTION__);
        static::a();
    }

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
        Coroutine::yield();
    }
}

$coroutine = Coroutine::run(function () {
    TestGetExecution::test();
});

echo $coroutine->getTraceDepth(), PHP_LF, PHP_LF;

echo $coroutine->getTraceAsString(), PHP_LF, PHP_LF;

$traceDepth = $coroutine->getTraceDepth();

for ($n = 0; $n < $traceDepth - 1; $n++) {
    echo $coroutine->eval('$tip', $n), PHP_LF;
}
for ($n = 0; $n < $traceDepth - 1; $n++) {
    Assert::same($coroutine->getDefinedVars($n)['tip'], $coroutine->eval('$tip', $n));
}

echo PHP_LF;

for ($n = 1; $n < $traceDepth - 1; $n++) {
    $coroutine->setLocalVar('tip2', "I am #{$n}", $n);
}
for ($n = 1; $n < $traceDepth - 1; $n++) {
    echo $coroutine->eval('$tip  . \', and \' . $tip2', $n), PHP_LF;
}
for ($n = 1; $n < $traceDepth - 1; $n++) {
    Assert::same($coroutine->getDefinedVars($n)['tip2'], $coroutine->eval('$tip2', $n));
}

echo PHP_LF;

$coroutine->resume();


echo 'Done' . PHP_LF;

?>
--EXPECTF--
6

#0 %sexecution.php(%d): Swow\Coroutine::yield()
#1 %sexecution.php(%d): TestGetExecution::c()
#2 %sexecution.php(%d): TestGetExecution::b()
#3 %sexecution.php(%d): TestGetExecution::a()
#4 %sexecution.php(%d): TestGetExecution::test()
#5 [internal function]: {closure}()
#6 {main}

I am C
I am C
I am B
I am A
I am TEST

I am C, and I am #1
I am B, and I am #2
I am A, and I am #3
I am TEST, and I am #4

Done
