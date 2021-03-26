--TEST--
swow_coroutine: run
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(function (...$data) {
    echo implode('', $data) . PHP_LF;
    var_dump(Swow\Coroutine::getCurrent());
    $leak = new stdClass;
    echo 'Out' . PHP_LF;
    Swow\Coroutine::yield($leak); // no one received (should use Channel)
    echo 'Never here' . PHP_LF;
    var_dump($leak);
}, 'I', 'n');
var_dump($coroutine);
echo 'Kill' . PHP_LF;
$coroutine->kill();
var_dump($coroutine);
echo 'Done' . PHP_LF;

?>
--EXPECTF--
In
object(Swow\Coroutine)#%d (%d) {
  ["id"]=>
  int(%d)
  ["state"]=>
  string(7) "running"
  ["round"]=>
  int(%d)
  ["elapsed"]=>
  string(%d) "%s"
  ["trace"]=>
  string(%d) "
#0 [internal function]: Swow\Coroutine->__debugInfo()
#1 %s(%d): var_dump(Object(Swow\Coroutine))
#2 [internal function]: {closure}('I', 'n')
#3 {main}
"
}
Out
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
  string(%d) "
#0 %s(%d): Swow\Coroutine::yield(%s)
#1 [%s]: {closure}(%s)
#2 {main}
"
}
Kill
object(Swow\Coroutine)#%d (%d) {
  ["id"]=>
  int(%d)
  ["state"]=>
  string(4) "dead"
  ["round"]=>
  int(%d)
  ["elapsed"]=>
  string(%d) "%s"
}
Done
