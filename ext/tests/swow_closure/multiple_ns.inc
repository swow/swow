<?php

namespace NamespaceA {
    use \A\B as SomeImportedClass;
    const SOME_CONST = __NAMESPACE__ . '\SOME_CONST';
    class SomeClass
    {
        function __construct()
        {
            echo static::class . "::__construct\n";
        }
    }
    function someFunc()
    {
        echo __NAMESPACE__ . "\\someFunc()\n";
    }
    function getAnonymous()
    {
        return function () {
            new SomeImportedClass();
            echo __NAMESPACE__ . "\\\$anonymous()\n";
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}

namespace NamespaceB {
    use \A\C as SomeImportedClass;
    const SOME_CONST = __NAMESPACE__ . '\SOME_CONST';
    class SomeClass
    {
        function __construct()
        {
            echo static::class . "::__construct\n";
        }
    }
    function someFunc()
    {
        echo __NAMESPACE__ . "\\someFunc()\n";
    }
    function getAnonymous()
    {
        return function () {
            new SomeImportedClass();
            echo __NAMESPACE__ . "\\\$anonymous()\n";
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}

namespace {
    use \A\D as SomeImportedClass;
    const SOME_CONST = __NAMESPACE__ . '\SOME_CONST';
    class SomeClass
    {
        function __construct()
        {
            echo static::class . "::__construct\n";
        }
    }
    function someFunc()
    {
        echo __NAMESPACE__ . "\\someFunc()\n";
    }
    function getAnonymous()
    {
        return function () {
            new SomeImportedClass();
            echo __NAMESPACE__ . "\\\$anonymous()\n";
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}
