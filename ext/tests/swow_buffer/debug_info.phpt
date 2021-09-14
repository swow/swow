--TEST--
swow_buffer: bad arguments passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Socket;

class MyBuffer extends Buffer
{
    public function __construct(int $size = Buffer::DEFAULT_SIZE)
    {
        var_dump($this);
        parent::__construct($size);
    }
}

$buffer = new MyBuffer(0);
// empty buffer
var_dump($buffer);
// shared buffer
var_dump($buffer->write('a string that is referenced by this buffer'));

$buffer = new Buffer(8192);

Coroutine::run(function () use ($buffer) {
    $socket = new Socket(Socket::TYPE_UDP);
    $socket->bind('127.0.0.1');
    $socket->recvFrom($buffer);
    echo 'Never here' . PHP_LF;
});

// locked buffer
var_dump($buffer);

Coroutine::killAll();

echo "Done\n";
?>
--EXPECTF--
object(MyBuffer)#%d (%d) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
}
object(MyBuffer)#%d (%d) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
}
object(MyBuffer)#%d (%d) {
  ["value"]=>
  string(42) "a string that is referenced by this buffer"
  ["size"]=>
  int(42)
  ["length"]=>
  int(42)
  ["offset"]=>
  int(42)
  ["shared"]=>
  bool(true)
}
object(Swow\Buffer)#%d (%d) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(8192)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
  ["locked"]=>
  bool(true)
}
Done
