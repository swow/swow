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

$channel = new Swow\Channel(8);
var_dump($channel);

for ($l = 2; $l--;) {
    for ($n = 4; $n--;) {
        $channel->push(true);
    }
    var_dump($channel);
}

?>
--EXPECTF--
object(Swow\Channel)#%d (%d) {
  ["capacity"]=>
  int(0)
  ["length"]=>
  int(0)
  ["readable"]=>
  bool(false)
  ["writable"]=>
  bool(false)
}
object(Swow\Channel)#%d (%d) {
  ["capacity"]=>
  int(8)
  ["length"]=>
  int(0)
  ["readable"]=>
  bool(false)
  ["writable"]=>
  bool(true)
}
object(Swow\Channel)#%d (%d) {
  ["capacity"]=>
  int(8)
  ["length"]=>
  int(4)
  ["readable"]=>
  bool(true)
  ["writable"]=>
  bool(true)
}
object(Swow\Channel)#%d (%d) {
  ["capacity"]=>
  int(8)
  ["length"]=>
  int(8)
  ["readable"]=>
  bool(true)
  ["writable"]=>
  bool(false)
}
