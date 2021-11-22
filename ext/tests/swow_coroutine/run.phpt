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
#%d [internal function]: Swow\Coroutine->__debugInfo()
#%d %s(%d): var_dump(Object(Swow\Coroutine))
#%d [internal function]: {closure}('I', 'n')%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure), 'I', 'n')
#%d {main}
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
#%d %s(%d): Swow\Coroutine::yield(%s)
#%d [%s]: {closure}(%s)
#%d {main}
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
