--TEST--
swow_coroutine: getTraceAsList
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(static function (): void {
    (static function (): void {
        (static function (): void {
            (static function (): void {
                Swow\Coroutine::yield();
            })();
        })();
    })();
});
$trace = $coroutine->getTraceAsList();
var_dump($trace);
$coroutine->resume();

echo "Done\n";
?>
--EXPECTF--
array(6) {
  [0]=>
  string(%d) "%sgetTraceAsList.php(%d): Swow\Coroutine::yield()"
  [1]=>
  string(%d) "%sgetTraceAsList.php(%d): {closure}()"
  [2]=>
  string(%d) "%sgetTraceAsList.php(%d): {closure}()"
  [3]=>
  string(%d) "%sgetTraceAsList.php(%d): {closure}()"
  [4]=>
  string(32) "[internal function]: {closure}()"
  [5]=>
  string(6) "{main}"
}
Done
