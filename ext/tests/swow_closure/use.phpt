--TEST--
swow_closure: use keryword
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/use.inc';

[$file, $line] = $anonymous();
$anonymousString = serialize($anonymous);
phpt_var_dump($anonymousString);
$anonymousUnserialized = unserialize($anonymousString);
[$fileUnserialized, $lineUnserialized] = $anonymousUnserialized();
Assert::same($fileUnserialized, $file);
Assert::same($lineUnserialized, $line);

[$file, $line] = $arrow();
$arrowString = serialize($arrow);
phpt_var_dump($arrowString);
$arrowUnserialized = unserialize($arrowString);
[$fileUnserialized, $lineUnserialized] = $arrowUnserialized();
Assert::same($fileUnserialized, $file);
Assert::same($lineUnserialized, $line);

echo "Done\n";
?>
--EXPECT--
use.inc: $a
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
use.inc: $a
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
use.inc: $a
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
use.inc: $a
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
Done
