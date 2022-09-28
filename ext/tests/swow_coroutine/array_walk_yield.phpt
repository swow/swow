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

$coro = new Coroutine(static function () use ($list): void {
    array_walk($list, static function (array $v, string $k): void {
        echo "{$k} => [ k => {$v['k']} ]\n";
        Coroutine::yield(true);
    });
    array_walk_recursive($list, static function (string $v, string $k): void {
        echo "{$k} => {$v}\n";
        Coroutine::yield(true);
    });
    Coroutine::yield(false);
});

while ($coro->resume()) {
}
$coro->resume();

echo "Done\n";

?>
--EXPECT--
foo => [ k => v3 ]
bar => [ k => v2 ]
baz => [ k => v1 ]
k => v3
k => v2
k => v1
Done
