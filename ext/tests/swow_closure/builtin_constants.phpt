--TEST--
swow_closure: builtin constants
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$anonymous = static function (): void {
    var_dump(
        $_SERVER['argv'][0],
        $_POST,
        STDIN,
        STDOUT,
        STDERR,
    );
};

$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

$arrow = static fn () => $anonymous();

$arrowString = serialize($arrow);
// var_dump($arrowString);
$arrowUnserialized = unserialize($arrowString);
$arrowUnserialized();

echo "Done\n";
?>
--EXPECTF--
string(%d) "%sbuiltin_constants.php"
array(0) {
}
resource(%d) of type (stream)
resource(%d) of type (stream)
resource(%d) of type (stream)
string(%d) "%sbuiltin_constants.php"
array(0) {
}
resource(%d) of type (stream)
resource(%d) of type (stream)
resource(%d) of type (stream)
Done
