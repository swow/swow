<?php

namespace NamespaceA {
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
            echo __NAMESPACE__ . "\\\$anonymous()\n";
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}

namespace NamespaceB {
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
            echo __NAMESPACE__ . "\\\$anonymous()\n";
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}
