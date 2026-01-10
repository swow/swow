--TEST--
swow_closure: comments for closure
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

/** this is a doc comment */
$anonymous = static function () {
    return 'whatever';
};

$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);

$reflect = new ReflectionFunction($anonymous);
$comments = $reflect->getDocComment();
echo $comments . PHP_EOL;
$reflect = new ReflectionFunction($anonymousUnserialized);
$comments = $reflect->getDocComment();
echo $comments . PHP_EOL;

echo "Done\n";
?>
--EXPECTF--
/** this is a doc comment */
/** this is a doc comment */
Done
