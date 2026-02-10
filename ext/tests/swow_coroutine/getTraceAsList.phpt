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
  string(%d) "%sgetTraceAsList.php(%d): {closur%s}()"
  [2]=>
  string(%d) "%sgetTraceAsList.php(%d): {closur%s}()"
  [3]=>
  string(%d) "%sgetTraceAsList.php(%d): {closur%s}()"
  [4]=>
  string(%d) "[internal function]: {closur%s}()"
  [5]=>
  string(6) "{main}"
}
Done
