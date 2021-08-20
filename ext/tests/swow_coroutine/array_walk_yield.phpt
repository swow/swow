--TEST--
swow_coroutine: context switch on array_walk
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$list = [
    'foo' => [
        'k' => 'v3',
    ], 'bar' => [
        'k' => 'v2',
    ], 'baz' => [
        'k' => 'v1',
    ],
];

$coro = new Coroutine(function () use ($list) {
    array_walk($list, function (array $v, string $k) {
        echo "{$k} => [ k => {$v['k']} ]" . PHP_LF;
        Coroutine::yield(true);
    });
    array_walk_recursive($list, function (string $v, string $k) {
        echo "{$k} => {$v}" . PHP_LF;
        Coroutine::yield(true);
    });
    Coroutine::yield(false);
});

while ($coro->resume());
$coro->resume();

echo 'Done' . PHP_LF;

?>
--EXPECT--
foo => [ k => v3 ]
bar => [ k => v2 ]
baz => [ k => v1 ]
k => v3
k => v2
k => v1
Done
