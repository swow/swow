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

namespace Swow\TestUtils {
    use function base64_encode;
    use function getenv;
    use function openssl_random_pseudo_bytes;
    use function random_bytes;
    use function str_replace;
    use function substr;

    function getRandomBytes(int $length = 64): string
    {
        if (\function_exists('openssl_random_pseudo_bytes')) {
            $random = openssl_random_pseudo_bytes($length);
        } else {
            $random = random_bytes($length);
        }
        $random = base64_encode($random);
        $random = str_replace(['/', '+', '='], ['x', 'y', 'z'], $random);

        return substr($random, 0, $length);
    }

    final class Testing
    {
        public const PRESSURE_DEBUG = 0;
        public const PRESSURE_MIN = 1;
        public const PRESSURE_LOW = 2;
        public const PRESSURE_MID = 3;
        public const PRESSURE_NORMAL = 4;

        public static int $pressureLevel;

        public static int $maxConcurrencyLow;
        public static int $maxConcurrencyMid;
        public static int $maxConcurrency;

        public static int $maxRequestsLow;
        public static int $maxRequestsMid;
        public static int $maxRequests;

        public static int $maxLengthLow;
        public static int $maxLength;
        public static int $maxLengthHigh;

        public static int $maxLoops;

        public static int $maxProcesses;

        public static function init(): void
        {
            if (getenv('TEST_PRESSURE_DEBUG') === '1') {
                self::$pressureLevel = self::PRESSURE_DEBUG;
            } elseif (getenv('USE_ZEND_ALLOC') === '0') {
                self::$pressureLevel = self::PRESSURE_LOW;
            } else {
                self::$pressureLevel = self::PRESSURE_NORMAL;
            }
            self::$maxConcurrencyLow = [1, 2, 4, 8, 16][self::$pressureLevel];
            self::$maxConcurrencyMid = [1, 4, 8, 16, 32][self::$pressureLevel];
            self::$maxConcurrency = [1, 8, 16, 32, 64][self::$pressureLevel];
            self::$maxRequestsLow = [1, 4, 8, 16, 32][self::$pressureLevel];
            self::$maxRequestsMid = [1, 8, 16, 32, 64][self::$pressureLevel];
            self::$maxRequests = [1, 16, 32, 64, 128][self::$pressureLevel];
            self::$maxLengthLow = [8, 8, 16, 32, 64][self::$pressureLevel];
            self::$maxLength = [64, 64, 256, 512, 1024][self::$pressureLevel];
            self::$maxLengthHigh = [256, 256, 1024, 8192, 65535, 262144][self::$pressureLevel];
            self::$maxLoops = (int) ([0.001, 1, 10, 100, 1000][self::$pressureLevel] * 100);
            self::$maxProcesses = [1, 2, 4, 8, 16][self::$pressureLevel];
        }
    }
}

namespace {
    use Swow\TestUtils\Testing;

    Testing::init();
}
