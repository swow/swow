--TEST--
swow_closure: multiple namespace
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require_once __DIR__ . '/multiple_ns.inc';

$anonymous = NamespaceA\getAnonymous();

$anonymous();
$anonymousString = serialize($anonymous);
var_dump($anonymousString);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

$anonymous = NamespaceB\getAnonymous();

$anonymous();
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();


echo 'Done' . PHP_LF;
?>
--XFAIL--
how?
--EXPECT--
NamespaceA\$anonymous()
string(21) "NamespaceA\SOME_CONST"
NamespaceA\SomeClass::__construct
NamespaceA\someFunc()
NamespaceA\$anonymous()
string(21) "NamespaceA\SOME_CONST"
NamespaceA\SomeClass::__construct
NamespaceA\someFunc()
NamespaceB\$anonymous()
string(21) "NamespaceB\SOME_CONST"
NamespaceB\SomeClass::__construct
NamespaceB\someFunc()
NamespaceB\$anonymous()
string(21) "NamespaceB\SOME_CONST"
NamespaceB\SomeClass::__construct
NamespaceB\someFunc()
Done