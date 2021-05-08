--TEST--
swow_fs: phar functionality
--SKIPIF--
<?php
/*
require __DIR__ . '/../include/skipif.php';
$loaded1 = shell_exec(PHP_BINARY . " -m");
if(false === strpos($loaded1, "Swow")){
    $loaded2 = shell_exec(PHP_BINARY . " -dextension=swow -ri swow -r \"exit(0);\" ");
    if(
        false === strpos($loaded2, "Swow") ||
        false !== strpos($loaded2, "Warning")
    ){
        skip("Swow is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension=swow", true);
    }
}
*/
?>
--INI--
phar.readonly=0
--FILE--
<?php

$loaded1 = shell_exec(PHP_BINARY . " -m");
if(false === strpos($loaded1, "Swow")){
    var_dump($loaded1);
    $loaded2 = shell_exec(PHP_BINARY . " -dextension=swow -ri swow");
    if(
        false === strpos($loaded2, "Swow") ||
        false !== strpos($loaded2, "Warning")
    ){
        var_dump($loaded2);
        //skip("Swow is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension=swow", true);
    }
}
exit(1);

require_once __DIR__ . "/build_phar.inc";

$ext_enable = " ";
$loaded = shell_exec(PHP_BINARY . " -m");
if(strpos($loaded, "Swow") < 0){
    $ext_enable = " -dextension=swow ";
}

build_phar(__DIR__ . DIRECTORY_SEPARATOR . "phartest.phar", TEST_AUTOLOAD);
rename(__DIR__ . DIRECTORY_SEPARATOR . "phartest.phar", __DIR__ . DIRECTORY_SEPARATOR . "phartest");
passthru(PHP_BINARY . $ext_enable . __DIR__ . DIRECTORY_SEPARATOR ."phartest");

build_phar(__DIR__ . DIRECTORY_SEPARATOR . "phartest2.phar", TEST_REQUIRE);
rename(__DIR__ . DIRECTORY_SEPARATOR . "phartest2.phar", __DIR__ . DIRECTORY_SEPARATOR . "phartest2");
passthru(PHP_BINARY . $ext_enable . __DIR__ . DIRECTORY_SEPARATOR ."phartest2");

build_phar(__DIR__ . DIRECTORY_SEPARATOR . "phartest3.phar", TEST_INCLUDE);
rename(__DIR__ . DIRECTORY_SEPARATOR . "phartest3.phar", __DIR__ . DIRECTORY_SEPARATOR . "phartest3");
passthru(PHP_BINARY . $ext_enable . __DIR__ . DIRECTORY_SEPARATOR ."phartest3");

?>
--CLEAN--
<?php
unlink(__DIR__ . "/phartest.phar");
unlink(__DIR__ . "/phartest");
unlink(__DIR__ . "/phartest2.phar");
unlink(__DIR__ . "/phartest2");
unlink(__DIR__ . "/phartest3.phar");
unlink(__DIR__ . "/phartest3");
?>
--EXPECT--
on run.php
Class1 initializing
Class1 run() is called
Class2 initializing
Class2 run() is called
Class1 initializing
on run.php
Class1 initializing
Class1 run() is called
Class2 initializing
Class2 run() is called
Class1 initializing
on run.php
Class1 initializing
Class1 run() is called
Class2 initializing
Class2 run() is called
Class1 initializing
