--TEST--
swow_closure: inline html
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/inline_html.inc';

$anonymous();
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

echo "Done\n";
?>
--EXPECTF--
hello world
!!!anonymous
anonymous
Done
