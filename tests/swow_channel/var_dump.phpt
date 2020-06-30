--TEST--
swow_channel: var_dump
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$channel = new Swow\Channel;
var_dump($channel);

$channel = new Swow\Channel(100);
for ($n = 10; $n--;) {
    $channel->push(true);
}
var_dump($channel);

?>
--EXPECTF--
object(Swow\Channel)#%d (2) {
  ["capacity"]=>
  int(0)
  ["length"]=>
  int(0)
}
object(Swow\Channel)#%d (2) {
  ["capacity"]=>
  int(100)
  ["length"]=>
  int(10)
}
