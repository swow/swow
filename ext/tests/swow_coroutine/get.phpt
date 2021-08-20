--TEST--
swow_coroutine: get
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

const N = 10;

$coroutines = [];

for ($n = N; $n--;) {
    $coroutines[] = new Coroutine(function () {
        Coroutine::yield(Coroutine::getCurrent()->getId());
    });
}

foreach ($coroutines as $coro) {
    $id = $coro->resume();
    if (Coroutine::get($id) !== $coro) {
        echo "Coroutine {$id} is not same";
        var_dump(Coroutine::get($id), $coro);
    }
    $coro->resume();
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
