<?php

# PHP
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
ini_set('report_memleaks', 1);
ini_set('memory_limit', '64M');

error_reporting(E_ALL);

define('PHP_LF', "\n");
/** ============== Env =============== */
define('USE_VALGRIND', getenv('USE_ZEND_ALLOC') === '0');
/** ============== Pressure ============== */
define('PRESSURE_DEBUG', 0);
define('PRESSURE_MIN', 1);
define('PRESSURE_LOW', 2);
define('PRESSURE_MID', 3);
define('PRESSURE_NORMAL', 4);
define('PRESSURE_LEVEL',
    (getenv('PRESSURE_DEBUG') == 1) ? PRESSURE_DEBUG :
        (USE_VALGRIND ? PRESSURE_LOW :
            ((0) ? PRESSURE_MID : PRESSURE_NORMAL)));
/** ============== Count ============== */
define('MAX_CONCURRENCY', [1, 16, 32, 64, 256][PRESSURE_LEVEL]);
define('MAX_CONCURRENCY_MID', [1, 8, 16, 32, 128][PRESSURE_LEVEL]);
define('MAX_CONCURRENCY_LOW', [1, 4, 8, 16, 64][PRESSURE_LEVEL]);
define('MAX_REQUESTS', [1, 16, 32, 64, 128][PRESSURE_LEVEL]);
define('MAX_REQUESTS_MID', [1, 8, 16, 32, 64][PRESSURE_LEVEL]);
define('MAX_REQUESTS_LOW', [1, 4, 8, 16, 32][PRESSURE_LEVEL]);
define('MAX_LENGTH_HIGH', [256, 256, 1024, 8192, 65535, 262144][PRESSURE_LEVEL]);
define('MAX_LENGTH', [64, 64, 256, 512, 1024][PRESSURE_LEVEL]);
define('MAX_LENGTH_LOW', [8, 8, 16, 32, 64][PRESSURE_LEVEL]);
define('MAX_LOOPS', (int) ([0.001, 1, 10, 100, 1000][PRESSURE_LEVEL] * 100));
define('MAX_PROCESS_NUM', [1, 1, 2, 4, 8][PRESSURE_LEVEL]);

# Swow

// TODO: remove it
ini_set('swow.better_error_info', true);

# Test tools

// TODO: move it to library
require __DIR__ . '/Assert.php';

function getRandomBytes(int $length = 64, bool $original = false)
{
    $_length = $original ? $length : $length * 2;
    if (function_exists('openssl_random_pseudo_bytes')) {
        $random = openssl_random_pseudo_bytes($_length);
    } else {
        $random = random_bytes($_length);
    }
    $random = base64_encode($random);
    if (!$original) {
        $random = preg_replace('/[\/+=]/', '', $random);
        $random = substr($random, 0, $length);
    }

    return $random;
}

function getRandomBytesArray(int $size, int $length = 64)
{
    $randoms = [];
    while ($size--) {
        $randoms[] = getRandomBytes($length);
    }

    return $randoms;
}

function switchProcess(): void
{
    usleep(100 * 1000);
}

function isInTest(): bool
{
    global $argv;
    return !(substr($argv[0], -5) === '.phpt');
}

function phpt_sprintf(...$args): void
{
    if (isInTest()) {
        return;
    }
    sprintf(...$args);
}

function phpt_var_dump(...$args): void
{
    if (isInTest()) {
        return;
    }
    var_dump(...$args);
}
