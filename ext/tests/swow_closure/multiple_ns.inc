<?php

namespace NamespaceA {
    use \A\B as SomeImportedClass;
    use \A\C,\A\D;
    use function \A\E, \A\F;
    use const \A\G as constG, \A\H as constH;
    use \A\{I,};
    use \A\{J,K};
    use \A\L\M\{
        N as NN,
        O as OO,
    };
    use function \A\L\M\{
        P as PP,
        Q as QQ,
    };
    use const \A\L\M\{
        R as RR,
        S as SS,
    };
    use \A\L\M\{
        const T as TT,
        function U as UU,
        V as VV,
    };
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
            new SomeImportedClass();
            new C();
            new D();
            E();
            F();
            var_dump(constG, constH);
            new I();
            new J();
            new K();
            new NN();
            new OO();
            PP();
            QQ();
            var_dump(RR, SS, TT);
            UU();
            new VV();
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}

namespace NamespaceB {
    use \A\C as SomeImportedClass;
    use \A\B,\A\D;
    use function \A\E, \A\F;
    use const \A\G as constG, \A\H as constH;
    use \A\{I,};
    use \A\{J,K};
    use \A\L\M\{
        N as NN,
        O as OO,
    };
    use function \A\L\M\{
        P as PP,
        Q as QQ,
    };
    use const \A\L\M\{
        R as RR,
        S as SS,
    };
    use \A\L\M\{
        const T as TT,
        function U as UU,
        V as VV,
    };
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
            new B();
            new SomeImportedClass();
            new D();
            E();
            F();
            var_dump(constG, constH);
            new I();
            new J();
            new K();
            new NN();
            new OO();
            PP();
            QQ();
            var_dump(RR, SS, TT);
            UU();
            new VV();
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}

namespace {
    use \A\D as SomeImportedClass;
    use \A\B,\A\C;
    use function \A\E, \A\F;
    use const \A\G as constG, \A\H as constH;
    use \A\{I,};
    use \A\{J,K};
    use \A\L\M\{
        N as NN,
        O as OO,
    };
    use function \A\L\M\{
        P as PP,
        Q as QQ,
    };
    use const \A\L\M\{
        R as RR,
        S as SS,
    };
    use \A\L\M\{
        const T as TT,
        function U as UU,
        V as VV,
    };
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
            new B();
            new C();
            new SomeImportedClass();
            E();
            F();
            var_dump(constG, constH);
            new I();
            new J();
            new K();
            new NN();
            new OO();
            PP();
            QQ();
            var_dump(RR, SS, TT);
            UU();
            new VV();
            var_dump(SOME_CONST);
            new SomeClass();
            someFunc();
        };
    }
}
