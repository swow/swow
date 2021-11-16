--TEST--
swow_coroutine: getAll
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

const N = 1;

for ($n = N; $n--;) {
    Swow\Coroutine::run(function () {
        usleep(1000);
    });
}

Assert::same(Swow\Coroutine::count(), N + 1);

for ($n = N + 1; $n--;) {
    $coroutines = Swow\Coroutine::getAll();
    Assert::count($coroutines, N + 1);
    unset($coroutines[array_rand($coroutines)]);
    Assert::count($coroutines, N);
}

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
