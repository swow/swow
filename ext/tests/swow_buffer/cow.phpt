--TEST--
swow_buffer: COW
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$buffer = new Swow\Buffer(0);
echo 'write(\'interned string\')' . PHP_LF;
$buffer->write('interned string');
var_dump($buffer);
echo 'write(\' copy\')' . PHP_LF;
$buffer->write(' copy');
var_dump($buffer);
echo 'close()' . PHP_LF;
$buffer->close();
var_dump($buffer);
echo 'write(\'interned string\')' . PHP_LF;
$buffer->write('interned string');
var_dump($buffer);
echo 'realloc(strlen(\'interned\'))' . PHP_LF;
$buffer->realloc(strlen('interned'));
var_dump($buffer);
echo 'close()' . PHP_LF;
$buffer->close();
var_dump($buffer);
echo 'write(\'interned string\')' . PHP_LF;
$buffer->write('interned string');
var_dump($buffer);
echo 'extend()' . PHP_LF;
$buffer->extend();
var_dump($buffer);
echo 'mallocTrim()' . PHP_LF;
$buffer->mallocTrim();
var_dump($buffer);
echo 'close()' . PHP_LF;
$buffer->close();
var_dump($buffer);
echo 'write(\'interned string\')' . PHP_LF;
$buffer->write('interned string');
var_dump($buffer);
echo 'mallocTrim()' . PHP_LF;
$buffer->mallocTrim(); // same as do nothing because it's shared
var_dump($buffer);
echo 'close()' . PHP_LF;
$buffer->close();
var_dump($buffer);
echo 'str_repeat(\'x\', 6)' . PHP_LF;
$buffer->write(str_repeat('x', 6));
var_dump($buffer);
echo 'toString()' . PHP_LF;
$string = $buffer->toString();
var_dump($buffer);
echo 'clear()' . PHP_LF;
$buffer->seek(2)->write('yyzz');
var_dump($buffer);
echo 'str_replace(\'x\', \'s\', $string)' . PHP_LF;
$string2 = str_replace('x', 's', $string);
var_dump($string2);

?>
--EXPECTF--
write('interned string')
object(Swow\Buffer)#%d (5) {
  ["value"]=>
  string(15) "interned string"
  ["size"]=>
  int(15)
  ["length"]=>
  int(15)
  ["offset"]=>
  int(15)
  ["shared"]=>
  bool(true)
}
write(' copy')
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(20) "interned string copy"
  ["size"]=>
  int(32)
  ["length"]=>
  int(20)
  ["offset"]=>
  int(20)
}
close()
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
}
write('interned string')
object(Swow\Buffer)#%d (5) {
  ["value"]=>
  string(15) "interned string"
  ["size"]=>
  int(15)
  ["length"]=>
  int(15)
  ["offset"]=>
  int(15)
  ["shared"]=>
  bool(true)
}
realloc(strlen('interned'))
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(8) "interned"
  ["size"]=>
  int(8)
  ["length"]=>
  int(8)
  ["offset"]=>
  int(8)
}
close()
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
}
write('interned string')
object(Swow\Buffer)#%d (5) {
  ["value"]=>
  string(15) "interned string"
  ["size"]=>
  int(15)
  ["length"]=>
  int(15)
  ["offset"]=>
  int(15)
  ["shared"]=>
  bool(true)
}
extend()
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(15) "interned string"
  ["size"]=>
  int(32)
  ["length"]=>
  int(15)
  ["offset"]=>
  int(15)
}
mallocTrim()
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(15) "interned string"
  ["size"]=>
  int(15)
  ["length"]=>
  int(15)
  ["offset"]=>
  int(15)
}
close()
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
}
write('interned string')
object(Swow\Buffer)#%d (5) {
  ["value"]=>
  string(15) "interned string"
  ["size"]=>
  int(15)
  ["length"]=>
  int(15)
  ["offset"]=>
  int(15)
  ["shared"]=>
  bool(true)
}
mallocTrim()
object(Swow\Buffer)#%d (5) {
  ["value"]=>
  string(15) "interned string"
  ["size"]=>
  int(15)
  ["length"]=>
  int(15)
  ["offset"]=>
  int(15)
  ["shared"]=>
  bool(true)
}
close()
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
}
str_repeat('x', 6)
object(Swow\Buffer)#%d (5) {
  ["value"]=>
  string(6) "xxxxxx"
  ["size"]=>
  int(6)
  ["length"]=>
  int(6)
  ["offset"]=>
  int(6)
  ["shared"]=>
  bool(true)
}
toString()
object(Swow\Buffer)#%d (5) {
  ["value"]=>
  string(6) "xxxxxx"
  ["size"]=>
  int(6)
  ["length"]=>
  int(6)
  ["offset"]=>
  int(6)
  ["shared"]=>
  bool(true)
}
clear()
object(Swow\Buffer)#%d (4) {
  ["value"]=>
  string(6) "xxyyzz"
  ["size"]=>
  int(6)
  ["length"]=>
  int(6)
  ["offset"]=>
  int(6)
}
str_replace('x', 's', $string)
string(6) "ssssss"
