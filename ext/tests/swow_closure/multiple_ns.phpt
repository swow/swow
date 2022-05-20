--TEST--
swow_closure: multiple namespace
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/multiple_ns.inc';
require __DIR__ . '/use.inc.inc';

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
NamespaceA\$anonymous()
use.inc.inc: class B:__construct
use.inc.inc: class C:__construct
use.inc.inc: class D:__construct
use.inc.inc: func E
use.inc.inc: func F
string(7) "const G"
string(7) "const H"
use.inc.inc: class I:__construct
use.inc.inc: class J:__construct
use.inc.inc: class K:__construct
use.inc.inc: class N:__construct
use.inc.inc: class O:__construct
use.inc.inc: func P
use.inc.inc: func Q
string(7) "const R"
string(7) "const S"
string(7) "const T"
use.inc.inc: func U
use.inc.inc: class V:__construct
string(21) "NamespaceA\SOME_CONST"
NamespaceA\SomeClass::__construct
NamespaceA\someFunc()
NamespaceA\$anonymous()
use.inc.inc: class B:__construct
use.inc.inc: class C:__construct
use.inc.inc: class D:__construct
use.inc.inc: func E
use.inc.inc: func F
string(7) "const G"
string(7) "const H"
use.inc.inc: class I:__construct
use.inc.inc: class J:__construct
use.inc.inc: class K:__construct
use.inc.inc: class N:__construct
use.inc.inc: class O:__construct
use.inc.inc: func P
use.inc.inc: func Q
string(7) "const R"
string(7) "const S"
string(7) "const T"
use.inc.inc: func U
use.inc.inc: class V:__construct
string(21) "NamespaceA\SOME_CONST"
NamespaceA\SomeClass::__construct
NamespaceA\someFunc()
NamespaceB\$anonymous()
use.inc.inc: class B:__construct
use.inc.inc: class C:__construct
use.inc.inc: class D:__construct
use.inc.inc: func E
use.inc.inc: func F
string(7) "const G"
string(7) "const H"
use.inc.inc: class I:__construct
use.inc.inc: class J:__construct
use.inc.inc: class K:__construct
use.inc.inc: class N:__construct
use.inc.inc: class O:__construct
use.inc.inc: func P
use.inc.inc: func Q
string(7) "const R"
string(7) "const S"
string(7) "const T"
use.inc.inc: func U
use.inc.inc: class V:__construct
string(21) "NamespaceB\SOME_CONST"
NamespaceB\SomeClass::__construct
NamespaceB\someFunc()
NamespaceB\$anonymous()
use.inc.inc: class B:__construct
use.inc.inc: class C:__construct
use.inc.inc: class D:__construct
use.inc.inc: func E
use.inc.inc: func F
string(7) "const G"
string(7) "const H"
use.inc.inc: class I:__construct
use.inc.inc: class J:__construct
use.inc.inc: class K:__construct
use.inc.inc: class N:__construct
use.inc.inc: class O:__construct
use.inc.inc: func P
use.inc.inc: func Q
string(7) "const R"
string(7) "const S"
string(7) "const T"
use.inc.inc: func U
use.inc.inc: class V:__construct
string(21) "NamespaceB\SOME_CONST"
NamespaceB\SomeClass::__construct
NamespaceB\someFunc()
\$anonymous()
use.inc.inc: class B:__construct
use.inc.inc: class C:__construct
use.inc.inc: class D:__construct
use.inc.inc: func E
use.inc.inc: func F
string(7) "const G"
string(7) "const H"
use.inc.inc: class I:__construct
use.inc.inc: class J:__construct
use.inc.inc: class K:__construct
use.inc.inc: class N:__construct
use.inc.inc: class O:__construct
use.inc.inc: func P
use.inc.inc: func Q
string(7) "const R"
string(7) "const S"
string(7) "const T"
use.inc.inc: func U
use.inc.inc: class V:__construct
string(11) "\SOME_CONST"
SomeClass::__construct
\someFunc()
\$anonymous()
use.inc.inc: class B:__construct
use.inc.inc: class C:__construct
use.inc.inc: class D:__construct
use.inc.inc: func E
use.inc.inc: func F
string(7) "const G"
string(7) "const H"
use.inc.inc: class I:__construct
use.inc.inc: class J:__construct
use.inc.inc: class K:__construct
use.inc.inc: class N:__construct
use.inc.inc: class O:__construct
use.inc.inc: func P
use.inc.inc: func Q
string(7) "const R"
string(7) "const S"
string(7) "const T"
use.inc.inc: func U
use.inc.inc: class V:__construct
string(11) "\SOME_CONST"
SomeClass::__construct
\someFunc()
Done
