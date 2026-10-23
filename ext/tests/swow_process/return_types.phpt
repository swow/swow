--TEST--
swow_process: void return types
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;
use Swow\Process\ProcessInterface;

$kill = new ReflectionFunction('Swow\\Process\\kill');
var_dump((string) $kill->getReturnType());

$interfaceKill = new ReflectionMethod(ProcessInterface::class, 'kill');
var_dump((string) $interfaceKill->getReturnType());

$close = new ReflectionMethod(ChildProcess::class, 'close');
var_dump((string) $close->getReturnType());
?>
--EXPECT--
string(4) "void"
string(4) "void"
string(4) "void"
