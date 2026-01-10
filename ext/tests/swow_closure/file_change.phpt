--TEST--
swow_closure: file changed between serializing-unserializing
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
// NOTE: this usage is not supported, this is for avoiding segfault

$code1 = <<<'PHP'
<?php

$anonymous = function () {
    echo __NAMESPACE__ . "hello1\n";
};
PHP;

$code2 = <<<'PHP'
<?php

$anonymous = function () {
    echo __NAMESPACE__ . "hello2\n";
};
PHP;

$code3 = <<<'PHP'
<?php

namespace SomeNamespace;

$anonymous = function () {
    echo __NAMESPACE__ . "hello3\n";
};
PHP;

$code4 = <<<'PHP'
<?php namespace SomeNamespace;

$anonymous = function () {
    echo __NAMESPACE__ . "hello4\n";
};
PHP;

$code5 = <<<'PHP'
<?php
namespace {
    $anonymous = function () {
        echo __NAMESPACE__ . "hello5\n";
    };
}
PHP;

$code6 = '';

file_put_contents(__DIR__ . '/file_change_test.php', $code1);

require __DIR__ . '/file_change_test.php';

$anonymous(); // hello1
// at first, hello is hello1
$anonymousString = serialize($anonymous);
// file changed, use hello2
file_put_contents(__DIR__ . '/file_change_test.php', $code2);
// unserialize is not affected
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized(); // hello1

// this will read as hello2
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized(); // hello2

file_put_contents(__DIR__ . '/file_change_test.php', $code3);
// this will fail, because line changed
Assert::throws(static function () use ($anonymous): void {
    serialize($anonymous);
}, 'Error'); // TODO: a normalized error

file_put_contents(__DIR__ . '/file_change_test.php', $code4);
// this will be ok, but result is wrong, because namespace changed
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized(); // hello4

file_put_contents(__DIR__ . '/file_change_test.php', $code5);
// this will be ok, but result is wrong
$anonymousString = serialize($anonymous);
$anonymousUnserialized = unserialize($anonymousString);
$anonymousUnserialized(); // hello5

file_put_contents(__DIR__ . '/file_change_test.php', $code6);
// this will fail
Assert::throws(static function () use ($anonymous): void {
    serialize($anonymous);
}, 'Error'); // TODO: a normalized error

echo "Done\n";
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/file_change_test.php');
?>
--EXPECTF--
hello1
hello1
hello2
SomeNamespacehello4
hello5
Done
