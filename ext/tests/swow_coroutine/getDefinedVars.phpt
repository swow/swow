--TEST--
swow_coroutine: getDefinedVars
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$remote_coro = Coroutine::run(function () {
    $var_a = 1;
    $var_b = 'string';
    Coroutine::yield();
    var_dump($var_a, $var_b);
});
try {
    var_dump($remote_coro->getDefinedVars());
} finally {
    $remote_coro->resume();
    echo 'Done' . PHP_LF;
}

?>
--EXPECT--
array(2) {
  ["var_a"]=>
  int(1)
  ["var_b"]=>
  string(6) "string"
}
int(1)
string(6) "string"
Done
