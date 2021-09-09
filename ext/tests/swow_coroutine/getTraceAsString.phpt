--TEST--
swow_coroutine: getTraceAsString
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

class A
{
}

$coroutine = Swow\Coroutine::run(function () {
    (function () {
        $resource = STDIN;
        $object = new A();
        $null = null;
        $array = [];
        $true = true;
        $false = false;
        $string = 'string';
        $long_string = 'a string that is very very very very very long';
        $long = 0x5f3759df;
        $double = 0.0072973526;
        (function ($resource, $object, $null, $array, $true, $false, $string, $long_string, $long, $double) {
            (function () {
                Swow\Coroutine::yield();
            })();
        })($resource, $object, $null, $array, $true, $false, $string, $long_string, $long, $double);
    })();
});
echo $coroutine->getTraceAsString() . PHP_LF;
$coroutine->resume();

echo 'Done' . PHP_LF;

?>
--EXPECTF--
#0 %s(%d): Swow\Coroutine::yield()
#1 %s(%d): {closure}()
#2 %s(%d): {closure}(Resource id #%d, Object(A), NULL, Array, true, false, 'string', 'a string that i...', 1597463007, 0.0072973526)
#3 %s(%d): {closure}()
#4 [internal function]: {closure}()
#5 {main}
Done
