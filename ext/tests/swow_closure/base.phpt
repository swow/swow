--TEST--
swow_closure: basic usage
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$anonymous = function () {
    echo 'anonymous' . PHP_LF;
};

var_dump($anonymous);
$anonymousString = serialize($anonymous);
var_dump($anonymousString);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

$arrow = fn () => print('arrow' . PHP_LF);

var_dump($arrow);
$arrowString = serialize($arrow);
var_dump($arrowString);
$arrowUnserialized = unserialize($arrowString);
$arrowUnserialized();

echo 'Done' . PHP_LF;
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
