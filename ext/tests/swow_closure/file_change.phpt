--TEST--
swow_closure: file changed between serializing-unserializing
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
// NOTE: this usage is not supported, this is for avoiding segfault

$code1 = <<<'PHP'
<?php

$anonymous = function () {
    echo "hello1\n";
};
PHP;

$code2 = <<<'PHP'
<?php

$anonymous = function () {
    echo "hello2\n";
};
PHP;

$code3 = <<<'PHP'
<?php

namespace SomeNamespace;

$anonymous = function () {
    echo "hello3\n";
};
PHP;

$code4 = <<<'PHP'
<?php namespace SomeNamespace;

$anonymous = function () {
    echo "hello4\n";
};
PHP;

$code5 = <<<'PHP'
<?php
namespace {
    $anonymous = function () {
        echo "hello5\n";
    };
}
PHP;

$code6 = '';

file_put_contents(__DIR__ . '/file_change.inc', $code1);

require __DIR__ . '/file_change.inc';

$anonymous(); // hello1
// at first, hello is hello1
$anonymousString = serialize($anonymous);
// file changed, use hello2
file_put_contents(__DIR__ . '/file_change.inc', $code2);
// unserialize is not affected
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized(); // hello1

// this will read as hello2
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized(); // hello2

file_put_contents(__DIR__ . '/file_change.inc', $code3);
// this will fail, because line changed
Assert::throws(function () use ($anonymous) {
    serialize($anonymous);
}, 'Error'); // TODO: a normalized error

file_put_contents(__DIR__ . '/file_change.inc', $code4);
// this will fail, because namespace changed
Assert::throws(function () use ($anonymous) {
    serialize($anonymous);
}, 'Error'); // TODO: a normalized error

file_put_contents(__DIR__ . '/file_change.inc', $code5);
// this will be ok, but result is wrong
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized(); // hello5

file_put_contents(__DIR__ . '/file_change.inc', $code6);
// this will fail
Assert::throws(function () use ($anonymous) {
    serialize($anonymous);
}, 'Error'); // TODO: a normalized error

echo "Done\n";
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/file_change.inc');
?>
--EXPECTF--
hello1
hello1
hello2
hello5
Done
