--TEST--
swow_closure: comments in closure
--XFAIL--
to hard to achieve this
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$anonymous = static function (): void {
    /** this is a doc comment */
    $a = static function () {
        return 'whatever';
    };

    $reflect = new ReflectionFunction($a);
    $comments = $reflect->getDocComment();
    echo $comments . PHP_EOL;
};

$anonymous();
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

echo "Done\n";
?>
--EXPECTF--
/** this is a doc comment */
/** this is a doc comment */
Done
