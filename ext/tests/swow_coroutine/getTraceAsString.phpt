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

$coroutine = Swow\Coroutine::run(static function (): void {
    (static function (): void {
        $resource = STDIN;
        $object = new A();
        $null = null;
        $array = [];
        $true = true;
        $false = false;
        $string = 'string';
        $long_string = 'a string that is very very very very very long';
        $long = 0x5F3759DF;
        $double = 0.0072973526;
        (static function ($resource, $object, $null, $array, $true, $false, $string, $long_string, $long, $double): void {
            (static function (): void {
                Swow\Coroutine::yield();
            })();
        })($resource, $object, $null, $array, $true, $false, $string, $long_string, $long, $double);
    })();
});
echo $coroutine->getTraceAsString() . "\n";
$coroutine->resume();

echo "Done\n";

?>
--EXPECTF--
#0 %s(%d): Swow\Coroutine::yield()
#1 %s(%d): {closur%s}()
#2 %s(%d): {closur%s}(Resource id #%d, Object(A), NULL, Array, true, false, 'string', 'a string that i...', 1597463007, 0.0072973526)
#3 %s(%d): {closur%s}()
#4 [internal function]: {closur%s}()
#5 {main}
Done
