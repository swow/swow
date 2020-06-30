--TEST--
swow_exceptions: exception
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

var_dump(new Swow\Exception('foo', 123));

?>
--EXPECTF--
object(Swow\Exception)#%d (%d) {
  ["message":protected]=>
  string(3) "foo"
  ["string":"Exception":private]=>
  string(0) ""
  ["code":protected]=>
  int(123)
  ["file":protected]=>
  string(%d) "%s"
  ["line":protected]=>
  int(%d)
  ["trace":"Exception":private]=>
  array(0) {
  }
  ["previous":"Exception":private]=>
  NULL
}
