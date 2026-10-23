--TEST--
swow_thannel: queue, close and cross-thread rendezvous
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$thannel = new Swow\Thannel(2);
var_dump($thannel->getCapacity());
var_dump($thannel->isEmpty());
var_dump($thannel->isFull());
var_dump($thannel->willBlock());

$thannel->push('a');
var_dump($thannel->getLength());
var_dump($thannel->isFull());
var_dump($thannel->willBlock());

$thannel->push('b');
var_dump($thannel->isFull());
var_dump($thannel->willBlock());

var_dump($thannel->pop(0));
var_dump($thannel->pop(0));
var_dump($thannel->isEmpty());

$unbuffered = new Swow\Thannel(0);
var_dump($unbuffered->getCapacity());
var_dump($unbuffered->isFull());
var_dump($unbuffered->willBlock());

$unlimited = new Swow\Thannel(-1);
var_dump($unlimited->getCapacity());

try {
    new Swow\Thannel(-2);
} catch (ValueError $e) {
    echo "capacity error\n";
}

$thannel = new Swow\Thannel(1);
try {
    $thannel->pop(10);
} catch (Swow\ThannelException $e) {
    echo "pop timeout\n";
}

$thannel = new Swow\Thannel(0);
try {
    $thannel->push('x', 10);
} catch (Swow\ThannelException $e) {
    echo "push timeout\n";
}

$thannel = new Swow\Thannel(2);
$thannel->push('a');
$thannel->push('b');
$thannel->close();
var_dump($thannel->isClosed());
var_dump($thannel->pop(0));
var_dump($thannel->pop(0));
try {
    $thannel->pop(0);
} catch (Swow\ThannelException $e) {
    echo "closed\n";
}
try {
    $thannel->push('c', 0);
} catch (Swow\ThannelException $e) {
    echo "push closed\n";
}

$thannel = new Swow\Thannel(1);
$copy = unserialize(serialize($thannel));
$copy->push('copy');
var_dump($thannel->pop(0));

$thannel = new Swow\Thannel(0);
$thread = new Swow\Thread(static function (Swow\Thannel $thannel): void {
    $thannel->push('hello');
}, $thannel);
$thread->start();
usleep(50000);
var_dump($thannel->pop(1000));
$thread->wait();

$thannel = new Swow\Thannel(2);
$thannel->push('x');
var_dump($thannel->__debugInfo());
?>
--EXPECT--
int(2)
bool(true)
bool(false)
bool(true)
int(1)
bool(false)
bool(false)
bool(true)
bool(true)
string(1) "a"
string(1) "b"
bool(true)
int(0)
bool(true)
bool(true)
int(4294967295)
capacity error
pop timeout
push timeout
bool(true)
string(1) "a"
string(1) "b"
closed
push closed
string(4) "copy"
string(5) "hello"
array(3) {
  ["capacity"]=>
  int(2)
  ["length"]=>
  int(1)
  ["closed"]=>
  bool(false)
}
