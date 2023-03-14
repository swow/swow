<?php
/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace NamespaceA {
    use A\B as SomeImportedClass;
    use A\C;
    use A\D;
    use A\J;
    use A\K;
    use A\L\M\N as NN;
    use A\L\M\O as OO;
    use A\L\M\V as VV;
    use A\{I};

use function A\E;
    use function A\F;
use function A\L\M\P as PP;
    use function A\L\M\Q as QQ;
use function A\L\M\U as UU;

use const A\G as constG;
use const A\H as constH;
    use const A\L\M\R as RR;
use const A\L\M\S as SS;
use const A\L\M\T as TT;

const SOME_CONST = __NAMESPACE__ . '\SOME_CONST';
    class SomeClass
    {
        public function __construct()
        {
            echo static::class . "::__construct\n";
        }
    }
    function someFunc(): void
    {
        echo __NAMESPACE__ . "\\someFunc()\n";
    }
    function getAnonymous()
    {
        return static function (): void {
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
    use A\B;
    use A\C as SomeImportedClass;
    use A\D;
    use A\J;
    use A\K;
    use A\L\M\N as NN;
    use A\L\M\O as OO;
    use A\L\M\V as VV;
    use A\{I};

use function A\E;
    use function A\F;
use function A\L\M\P as PP;
    use function A\L\M\Q as QQ;
use function A\L\M\U as UU;

use const A\G as constG;
use const A\H as constH;
    use const A\L\M\R as RR;
use const A\L\M\S as SS;
use const A\L\M\T as TT;

const SOME_CONST = __NAMESPACE__ . '\SOME_CONST';
    class SomeClass
    {
        public function __construct()
        {
            echo static::class . "::__construct\n";
        }
    }
    function someFunc(): void
    {
        echo __NAMESPACE__ . "\\someFunc()\n";
    }
    function getAnonymous()
    {
        return static function (): void {
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
    use A\B;
    use A\C;
    use A\D as SomeImportedClass;
    use A\J;
    use A\K;
    use A\L\M\N as NN;
    use A\L\M\O as OO;
    use A\L\M\V as VV;
    use A\{I};

use function A\E;
    use function A\F;
use function A\L\M\P as PP;
    use function A\L\M\Q as QQ;
use function A\L\M\U as UU;

use const A\G as constG;
use const A\H as constH;
    use const A\L\M\R as RR;
use const A\L\M\S as SS;
use const A\L\M\T as TT;

const SOME_CONST = __NAMESPACE__ . '\SOME_CONST';
    class SomeClass
    {
        public function __construct()
        {
            echo static::class . "::__construct\n";
        }
    }
    function someFunc(): void
    {
        echo __NAMESPACE__ . "\\someFunc()\n";
    }
    function getAnonymous()
    {
        return static function (): void {
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
