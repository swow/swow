<?php

namespace DummyNamespace1 {
    const CONSTANT1 = 'NSC1';
    class DummyClass
    {
        const CONSTANT2 = 'NSCC1';
        const CONSTANT3 = 'NSCC2' . self::CONSTANT2;
        const CONSTANT4 = 'NSCC3' . self::CONSTANT3;
    }
}

namespace DummyNamespace2 {
    const CONSTANT1 = 'NSC2';
    class DummyClass
    {
        const CONSTANT2 = 'NSCC2';
        const CONSTANT3 = CONSTANT1 . '+1', CONSTANT4 = self::CONSTANT2 . '+1';
    }
}
