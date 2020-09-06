--TEST--
swow_coroutine: new
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () { });
var_dump($coroutine);

?>
--EXPECTF--
object(Swow\Coroutine)#%d (%d) {
  ["id"]=>
  int(2)
  ["state"]=>
  string(5) "ready"
  ["round"]=>
  int(%d)
  ["elapsed"]=>
  string(%d) "%s"
}
