--TEST--
swow_buffer: shared
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$buffer = new Swow\Buffer(0);
var_dump($buffer);
$json = json_encode(['foo' => 'bar']);
$buffer->write($json);
var_dump($buffer);
$buffer->seek(2)->write('cha');
var_dump($buffer);
var_dump($json);

if (memory_get_usage(true) === 0) {
    return;
}

$memoryUsages = [0, 0, 0];
$buffer->close();
$bigString = str_repeat('X', $buffer::PAGE_SIZE);
$memoryUsages[0] = memory_get_usage();
$buffer->write($bigString);
$memoryUsages[1] = memory_get_usage();
$buffer->rewind()->write('Y');
$memoryUsages[2] = memory_get_usage();
Assert::lessThan($memoryUsages[1] - $memoryUsages[0], $buffer::PAGE_SIZE / 2); // 0 memory copy
Assert::greaterThanEq($memoryUsages[2] - $memoryUsages[1], $buffer::PAGE_SIZE); // unshared (copy on write)

$buffer->close();
$memoryUsages[0] = memory_get_usage();
$buffer->write($bigString);
$memoryUsages[1] = memory_get_usage();
$bigString = null;
$buffer->rewind()->write('Y');
$memoryUsages[2] = memory_get_usage();
Assert::lessThan($memoryUsages[1] - $memoryUsages[0], $buffer::PAGE_SIZE / 2); // 0 memory copy
Assert::lessThan($memoryUsages[2] - $memoryUsages[1], $buffer::PAGE_SIZE / 2); // 0 memory copy (ownership changed)

echo 'Done' . PHP_LF;

?>
--EXPECTF--
object(Swow\Buffer)#%d (%d) {
  ["value"]=>
  string(0) ""
  ["size"]=>
  int(0)
  ["length"]=>
  int(0)
  ["offset"]=>
  int(0)
}
object(Swow\Buffer)#%d (%d) {
  ["value"]=>
  string(13) "{"foo":"bar"}"
  ["size"]=>
  int(13)
  ["length"]=>
  int(13)
  ["offset"]=>
  int(13)
  ["shared"]=>
  bool(true)
}
object(Swow\Buffer)#%d (%d) {
  ["value"]=>
  string(13) "{"cha":"bar"}"
  ["size"]=>
  int(13)
  ["length"]=>
  int(13)
  ["offset"]=>
  int(5)
}
string(13) "{"foo":"bar"}"
Done
