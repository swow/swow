--TEST--
swow_coroutine: getTrace
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(function () {
    (function () {
        (function () {
            (function () {
                Swow\Coroutine::yield();
            })();
        })();
    })();
});
$trace = $coroutine->getTrace();
$coroutine->resume();

var_dump($trace);

echo 'Done' . PHP_LF;
?>
--EXPECTF--
array(5) {
  [0]=>
  array(6) {
    ["file"]=>
    string(%d) "%sgetTrace.php"
    ["line"]=>
    int(%d)
    ["function"]=>
    string(5) "yield"
    ["class"]=>
    string(14) "Swow\Coroutine"
    ["type"]=>
    string(2) "::"
    ["args"]=>
    array(0) {
    }
  }
  [1]=>
  array(4) {
    ["file"]=>
    string(%d) "%sgetTrace.php"
    ["line"]=>
    int(%d)
    ["function"]=>
    string(9) "{closure}"
    ["args"]=>
    array(0) {
    }
  }
  [2]=>
  array(4) {
    ["file"]=>
    string(%d) "%sgetTrace.php"
    ["line"]=>
    int(%d)
    ["function"]=>
    string(9) "{closure}"
    ["args"]=>
    array(0) {
    }
  }
  [3]=>
  array(4) {
    ["file"]=>
    string(%d) "%sgetTrace.php"
    ["line"]=>
    int(%d)
    ["function"]=>
    string(9) "{closure}"
    ["args"]=>
    array(0) {
    }
  }
  [4]=>
  array(2) {
    ["function"]=>
    string(9) "{closure}"
    ["args"]=>
    array(0) {
    }
  }
}
Done