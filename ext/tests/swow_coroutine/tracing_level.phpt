--TEST--
swow_coroutine: tracing level overflow
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$coro = Coroutine::getCurrent();

$coro->getTrace(-1);
$coro->getTraceAsList(-1);
$coro->getTraceAsString(-1);
$coro->getExecutedFilename(-1);
$coro->getExecutedFunctionName(-1);
$coro->getExecutedLineno(-1);
$coro->eval('$a=1;', -1);
$coro->getDefinedVars(-1);
$coro->setLocalVar('a', 'b', -1);

$coro->getDefinedVars(PHP_INT_MAX);
$coro->getTrace(PHP_INT_MAX);
$coro->getTraceAsList(PHP_INT_MAX);
$coro->getTraceAsString(PHP_INT_MAX);
$coro->getExecutedFilename(PHP_INT_MAX);
$coro->getExecutedFunctionName(PHP_INT_MAX);
$coro->getExecutedLineno(PHP_INT_MAX);
$coro->eval('$a=1;', PHP_INT_MAX);
$coro->getDefinedVars(PHP_INT_MAX);
$coro->setLocalVar('a', 'b', PHP_INT_MAX);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
