--TEST--
swow_closure: serialize function of class anonymous
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--XFAIL--
Need to fix
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$o = new class {
    public static function foo(): void
    {
    }
};
$c = Closure::fromCallable([$o, 'foo']);
$s = serialize($c);
// var_dump($s);
try {
    $c = unserialize($s);
} catch (Throwable $e) {
    printf("failed to unserialize: %s\n", $e->getMessage());
    var_dump($s);
}
// var_dump($c);

echo "Done\n";
?>
--EXPECT--
Done
