--TEST--
swow_coroutine: run
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(static function (...$data): void {
    echo implode('', $data) . "\n";
    var_dump(Swow\Coroutine::getCurrent());
    $leak = new stdClass();
    echo "Out\n";
    Swow\Coroutine::yield($leak); // no one received (should use Channel)
    echo "Never here\n";
    var_dump($leak);
}, 'I', 'n');
var_dump($coroutine);
echo "Kill\n";
$coroutine->kill();
var_dump($coroutine);
echo "Done\n";

?>
--EXPECTF--
In
object(Swow\Coroutine)#%d (%d) {
  ["id"]=>
  int(%d)
  ["state"]=>
  string(7) "running"
  ["switches"]=>
  int(%d)
  ["elapsed"]=>
  string(%d) "%s"
  ["trace"]=>
  string(%d) "
#%d [internal function]: Swow\Coroutine->__debugInfo()
#%d %s(%d): var_dump(Object(Swow\Coroutine))
#%d [internal function]: {closur%s}('I', 'n')%A
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
  ["switches"]=>
  int(%d)
  ["elapsed"]=>
  string(%d) "%s"
  ["trace"]=>
  string(%d) "
#%d %s(%d): Swow\Coroutine::yield(%s)
#%d [%s]: {closur%s}(%s)
#%d {main}
"
}
Kill
object(Swow\Coroutine)#%d (%d) {
  ["id"]=>
  int(%d)
  ["state"]=>
  string(4) "dead"
  ["switches"]=>
  int(%d)
  ["elapsed"]=>
  string(%d) "%s"
}
Done
