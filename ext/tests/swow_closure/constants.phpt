--TEST--
swow_closure: constants reference
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/constant.inc';

const CONSTANT1 = 1;
const CONSTANT2 = 2, CONSTANT3 = '3';
const CONSTANT4 = [
    '4' => 'four',
];
define('CONSTANT5', '5');
define('CONSTANT' . '6', '6');
define('CONSTANT7', define('CONSTANT8', '8') ? '7' : 'failed');
$name = 'CONSTANT9';
define($name, '9');

class DummyClass
{
    const CONSTANT1 = 5 + 5;
    const CONSTANT2 = self::CONSTANT1 + 5;
    const CONSTANT3 = CONSTANT2 + 5;
    const CONSTANT4 = \DummyNamespace2\DummyClass::class . \DummyNamespace1\DummyClass::CONSTANT4;
}

$anonymous = function () {
    var_dump(
        'anonymous',
        CONSTANT1,
        CONSTANT2,
        CONSTANT3,
        CONSTANT4,
        CONSTANT5,
        CONSTANT6,
        CONSTANT7,
        CONSTANT8,
        CONSTANT9,
        DummyClass::class,
        DummyClass::CONSTANT1,
        DummyClass::CONSTANT2,
        DummyClass::CONSTANT3,
        \DummyNamespace1\CONSTANT1,
        \DummyNamespace1\DummyClass::class,
        \DummyNamespace1\DummyClass::CONSTANT2,
        \DummyNamespace1\DummyClass::CONSTANT3,
        \DummyNamespace1\DummyClass::CONSTANT4,
        \DummyNamespace2\CONSTANT1,
        \DummyNamespace2\DummyClass::class,
        \DummyNamespace2\DummyClass::CONSTANT2,
        \DummyNamespace2\DummyClass::CONSTANT3,
        \DummyNamespace2\DummyClass::CONSTANT4,
    );
};

$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized();

$arrow = fn () => $anonymous();

$arrowString = serialize($arrow);
$arrowUnserialized = unserialize($arrowString);
$arrowUnserialized();

echo 'Done' . PHP_LF;
?>
--EXPECT--
string(9) "anonymous"
int(1)
int(2)
string(1) "3"
array(1) {
  [4]=>
  string(4) "four"
}
string(1) "5"
string(1) "6"
string(1) "7"
string(1) "8"
string(1) "9"
string(10) "DummyClass"
int(10)
int(15)
int(7)
string(4) "NSC1"
string(26) "DummyNamespace1\DummyClass"
string(5) "NSCC1"
string(10) "NSCC2NSCC1"
string(15) "NSCC3NSCC2NSCC1"
string(4) "NSC2"
string(26) "DummyNamespace2\DummyClass"
string(5) "NSCC2"
string(6) "NSC2+1"
string(7) "NSCC2+1"
string(9) "anonymous"
int(1)
int(2)
string(1) "3"
array(1) {
  [4]=>
  string(4) "four"
}
string(1) "5"
string(1) "6"
string(1) "7"
string(1) "8"
string(1) "9"
string(10) "DummyClass"
int(10)
int(15)
int(7)
string(4) "NSC1"
string(26) "DummyNamespace1\DummyClass"
string(5) "NSCC1"
string(10) "NSCC2NSCC1"
string(15) "NSCC3NSCC2NSCC1"
string(4) "NSC2"
string(26) "DummyNamespace2\DummyClass"
string(5) "NSCC2"
string(6) "NSC2+1"
string(7) "NSCC2+1"
Done
