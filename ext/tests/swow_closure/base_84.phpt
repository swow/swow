--TEST--
swow_closure: basic usage for PHP 8.4+
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
needs_php_version('>=', '8.4');
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
  ["name"]=>
  string(%d) "{closure:%s:4}"
  ["file"]=>
  string(%d) "%s"
  ["line"]=>
  int(4)
}
string(%d) "%a"
anonymous
object(Closure)#%d (%d) {
  ["name"]=>
  string(%d) "{closure:%s:14}"
  ["file"]=>
  string(%d) "%s"
  ["line"]=>
  int(14)
}
string(%d) "%a"
arrow
Done
