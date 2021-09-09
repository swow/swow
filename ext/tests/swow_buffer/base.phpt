--TEST--
swow_buffer: base
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$buffer = new Swow\Buffer();
var_dump($buffer->getSize());
var_dump($buffer->getLength());
$buffer->write('foo');
$buffer->write('bar');
$buffer->write('car');
var_dump($buffer->getLength());
$buffer->rewind();
while (!$buffer->eof()) {
    echo $buffer->read(3) . PHP_LF;
}
var_dump($buffer);

$_buffer = (string) $buffer; /* eq to __toString */
var_dump($_buffer);
$buffer->clear();
var_dump($_buffer);

$buffer->write('foobarcar');

$_buffer = $buffer->fetchString();
var_dump($_buffer);
$buffer->clear();
var_dump($_buffer);

?>
--EXPECTF--
int(8192)
int(0)
int(9)
foo
bar
car
object(Swow\Buffer)#%d (%d) {
  ["value"]=>
  string(9) "foobarcar"
  ["size"]=>
  int(%d)
  ["length"]=>
  int(9)
  ["offset"]=>
  int(9)
}
string(9) "foobarcar"
string(9) "foobarcar"
string(9) "foobarcar"
string(9) "foobarcar"
