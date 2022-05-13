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
require_once __DIR__ . '/use.inc.inc';

$anonymous = NamespaceA\getAnonymous();

$anonymous();
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

$anonymous = NamespaceB\getAnonymous();

$anonymous();
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

$anonymous = getAnonymous();

$anonymous();
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

echo 'Done' . PHP_LF;
?>
--EXPECT--
use.inc.inc: class B:__construct
NamespaceA\$anonymous()
string(21) "NamespaceA\SOME_CONST"
NamespaceA\SomeClass::__construct
NamespaceA\someFunc()
use.inc.inc: class B:__construct
NamespaceA\$anonymous()
string(21) "NamespaceA\SOME_CONST"
NamespaceA\SomeClass::__construct
NamespaceA\someFunc()
use.inc.inc: class C:__construct
NamespaceB\$anonymous()
string(21) "NamespaceB\SOME_CONST"
NamespaceB\SomeClass::__construct
NamespaceB\someFunc()
use.inc.inc: class C:__construct
NamespaceB\$anonymous()
string(21) "NamespaceB\SOME_CONST"
NamespaceB\SomeClass::__construct
NamespaceB\someFunc()
use.inc.inc: class D:__construct
\$anonymous()
string(11) "\SOME_CONST"
SomeClass::__construct
\someFunc()
use.inc.inc: class D:__construct
\$anonymous()
string(11) "\SOME_CONST"
SomeClass::__construct
\someFunc()
Done