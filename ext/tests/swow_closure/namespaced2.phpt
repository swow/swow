--TEST--
swow_closure: namespaced2
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php

namespace {
    require __DIR__ . '/../include/bootstrap.php';
    require __DIR__ . '/namespaced2.inc';

    $anonymous = static function () {
        echo __LINE__ . ':' . __NAMESPACE__ . ' ';
        return static function (): void {
            echo __LINE__ . ':' . __NAMESPACE__ . "\n";
        };
    };

    // a variable at root namespace
    printf("a variable at root namespace\n");
    $anonymous()();

    $anonymousString = serialize($anonymous); // no namespace
    $unserialized = unserialize($anonymousString);
    $unserialized()();

    $anonymousString = serialize($anonymous()); // no namespace
    $unserialized = unserialize($anonymousString);
    $unserialized();

    // a variable at another namespace, returned by function
    printf("a variable at another namespace, returned by function\n");
    A\getNamespacedClosureInNamespacedClosure()()();

    $anonymousString = serialize(A\getNamespacedClosureInNamespacedClosure()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized()();

    $anonymousString = serialize(A\getNamespacedClosureInNamespacedClosure()()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized();

    // a variable at another namespace, returned by function from the third namespace
    printf("a variable at another namespace, returned by function from the third namespace\n");
    B\getNamespacedClosureInAnotherNamespacedClosure()()()();

    $anonymousString = serialize(B\getNamespacedClosureInAnotherNamespacedClosure()); // NamespaceB
    $unserialized = unserialize($anonymousString);
    $unserialized()()();

    $anonymousString = serialize(B\getNamespacedClosureInAnotherNamespacedClosure()()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized()();

    $anonymousString = serialize(B\getNamespacedClosureInAnotherNamespacedClosure()()()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized();

    // at another namespace, returned by method
    printf("at another namespace, returned by method\n");
    $objA = new A\A();
    $objA->getAnonymousInNamespacedClass()()();

    $anonymousString = serialize($objA->getAnonymousInNamespacedClass()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized()();

    $anonymousString = serialize($objA->getAnonymousInNamespacedClass()()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized();

    // at another namespace, returned by method from the third namespace
    printf("at another namespace, returned by method from the third namespace\n");
    $objB = new B\A();
    $objB->getAnonymousInAnotherNamespacedClass()()()();

    $anonymousString = serialize($objB->getAnonymousInAnotherNamespacedClass()); // NamespaceB
    $unserialized = unserialize($anonymousString);
    $unserialized()()();

    $anonymousString = serialize($objB->getAnonymousInAnotherNamespacedClass()()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized()();

    $anonymousString = serialize($objB->getAnonymousInAnotherNamespacedClass()()()); // NamespaceA
    $unserialized = unserialize($anonymousString);
    $unserialized();
}

namespace C {
    $anonymous = static function () {
        echo __LINE__ . ':' . __NAMESPACE__ . ' ';
        return static function (): void {
            echo __LINE__ . ':' . __NAMESPACE__ . "\n";
        };
    };

    // a variable at namespace
    printf("a variable at namespace\n");
    $anonymous()();

    $anonymousString = serialize($anonymous); // NamespaceC
    $unserialized = unserialize($anonymousString);
    $unserialized()();

    $anonymousString = serialize($anonymous()); // NamespaceC
    $unserialized = unserialize($anonymousString);
    $unserialized();

    echo "Done\n";
}

?>
--EXPECTF--
a variable at root namespace
%d: %d:
%d: %d:
%d: %d:
a variable at another namespace, returned by function
%d:A %d:A %d:A
%d:A %d:A %d:A
%d:A %d:A %d:A
a variable at another namespace, returned by function from the third namespace
%d:B %d:B %d:A %d:A %d:A
%d:B %d:B %d:A %d:A %d:A
%d:B %d:B %d:A %d:A %d:A
%d:B %d:B %d:A %d:A %d:A
at another namespace, returned by method
%d:A %d:A %d:A
%d:A %d:A %d:A
%d:A %d:A %d:A
at another namespace, returned by method from the third namespace
%d:B %d:B %d:A %d:A %d:A
%d:B %d:B %d:A %d:A %d:A
%d:B %d:B %d:A %d:A %d:A
%d:B %d:B %d:A %d:A %d:A
a variable at namespace
%d:C %d:C
%d:C %d:C
%d:C %d:C
Done
