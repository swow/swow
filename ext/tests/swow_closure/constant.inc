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

namespace DummyNamespace1 {
    const CONSTANT1 = 'NSC1';
    class DummyClass
    {
        public const CONSTANT2 = 'NSCC1';
        public const CONSTANT3 = 'NSCC2' . self::CONSTANT2;
        public const CONSTANT4 = 'NSCC3' . self::CONSTANT3;
    }
}

namespace DummyNamespace2 {
    const CONSTANT1 = 'NSC2';
    class DummyClass
    {
        public const CONSTANT2 = 'NSCC2';
        public const CONSTANT3 = CONSTANT1 . '+1';
        public const CONSTANT4 = self::CONSTANT2 . '+1';
    }
}
