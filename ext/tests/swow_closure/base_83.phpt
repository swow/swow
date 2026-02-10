--TEST--
swow_closure: basic usage for PHP 8.3 or lower
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
needs_php_version('<', '8.4');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$anonymous = static function (): void {
    echo "anonymous\n";
};

var_dump($anonymous);
$anonymousString = serialize($anonymous);
var_dump($anonymousString);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

$arrow = static fn() => print "arrow\n";

var_dump($arrow);
$arrowString = serialize($arrow);
var_dump($arrowString);
$arrowUnserialized = unserialize($arrowString);
$arrowUnserialized();

echo "Done\n";
?>
--EXPECTF--
object(Closure)#%d (%d) {
}
string(%d) "%a"
anonymous
object(Closure)#%d (%d) {
}
string(%d) "%a"
arrow
Done
