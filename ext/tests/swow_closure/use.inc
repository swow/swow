<?php

// all usage of T_USE is here
// please keep this file format for confusing parser

require __DIR__ . '/use.inc.inc';

use \A\B;
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
// this seems to be good syntax ???
/*
use function \A\L\M\{
    const W as WW,
};
*/

trait SomeTrait1 {
    public function method() {
        return null;
    }
    public function method1() {
        return null;
    }
}

trait SomeTrait2 {
    public function method() {
        return null;
    }
    public function method2() {
        return null;
    }
}

trait SomeTrait3 {
    public function method3() {
        return null;
    }
    public function method33() {
        return null;
    }
    public function method333() {
        return null;
    }
}

class SomeClass {
    use SomeTrait2,SomeTrait3;
}

class SomeClass2 {
    use SomeTrait1, SomeTrait2 {
        SomeTrait2::method insteadof SomeTrait1;
    }
    use SomeTrait3 {
        SomeTrait3::method3 as public;
        SomeTrait3::method33 as private;
        SomeTrait3::method333 as protected;
    }
}

class Taint {

}

$anonymous = function () use ($argv) {
    echo "use.inc: \$a\n";
    new B();
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
    Assert::throws(fn() => new NotExist(), Error::class);

    return [__FILE__, __LINE__];
};

$arrow = fn () => $anonymous();

use Taint as NotExist;
