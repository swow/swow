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
    public function __construct(int $size = Buffer::COMMON_SIZE)
    {
        var_dump($this);
        parent::__construct($size);
    }
}

$buffer = new MyBuffer(0);
// empty buffer
var_dump($buffer);
// shared buffer
$buffer->append('a interned string that is referenced by this buffer');
ob_start();
debug_zval_dump($buffer);
$debugZvalDump = ob_get_flush();
if (PHP_VERSION_ID >= 80100) {
    Assert::endsWith(explode("\n", $debugZvalDump)[2], ' interned');
} else {
    Assert::endsWith(explode("\n", $debugZvalDump)[2], ' refcount(1)');
}

$buffer = new Buffer(8192);

Coroutine::run(static function () use ($buffer): void {
    $socket = new Socket(Socket::TYPE_UDP);
    $socket->bind('127.0.0.1');
    $socket->recvFrom($buffer);
    echo "Never here\n";
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
}
object(MyBuffer)#%d (%d) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
}
object(MyBuffer)#%d (%d) refcount(%d){
  ["value"]=>
  string(51) "a interned string that is referenced by this buffer" %s
  ["size"]=>
  int(51)
  ["length"]=>
  int(51)
}
object(Swow\Buffer)#%d (%d) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(8192)
  ["length"]=>
  int(0)
  ["locker"]=>
  object(Swow\Coroutine)#%d (%d) {
    ["id"]=>
    int(%d)
    ["state"]=>
    string(7) "waiting"
    ["round"]=>
    int(%d)
    ["elapsed"]=>
    string(%d) "%s"
    ["trace"]=>
    string(%s) "
#0 %s(%d): Swow\Socket->recvFrom(Object(Swow\Buffer))
#1 [internal function]: {closure}()
#2 {main}
"
  }
}
Done
