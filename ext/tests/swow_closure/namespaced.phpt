--TEST--
swow_closure: namespaced
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/namespaced.inc';

$anonymous = \SomeNamespace\getAnonymous();

$anonymous();
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

echo "Done\n";
?>
--EXPECTF--
im namespaced! namespaced.inc:SomeNamespace
im namespaced! namespaced.inc:SomeNamespace
Done
