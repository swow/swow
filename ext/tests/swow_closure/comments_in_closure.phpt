--TEST--
swow_closure: comments in closure
--XFAIL--
to hard to achieve this
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$anonymous = function () {
    /** this is a doc comment */
    $a = function () {
        return "whatever";
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
