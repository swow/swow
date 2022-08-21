--TEST--
swow_buffer: realloc from null
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;

$buffer = new Buffer(0);
$buffer->realloc(256);
var_dump($buffer);

echo 'Done' . PHP_LF;
?>
--EXPECTF--
object(Swow\Buffer)#%d (%d) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(256)
  ["length"]=>
  int(0)
}
Done
