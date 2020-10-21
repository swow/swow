<?php

require __DIR__ . '/../../vendor/autoload.php';

# PHP ini

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
ini_set('report_memleaks', 1);
ini_set('memory_limit', '64M');

error_reporting(E_ALL);

# constants

define('PHP_LF', "\n");
/** ============== Env =============== */
define('USE_VALGRIND', getenv('USE_ZEND_ALLOC') === '0');
/** ============== Pressure ============== */
define('TEST_PRESSURE_DEBUG', 0);
define('TEST_PRESSURE_MIN', 1);
define('TEST_PRESSURE_LOW', 2);
define('TEST_PRESSURE_MID', 3);
define('TEST_PRESSURE_NORMAL', 4);
define('TEST_PRESSURE_LEVEL',
    (getenv('TEST_PRESSURE_DEBUG') == 1) ? TEST_PRESSURE_DEBUG :
        (USE_VALGRIND ? TEST_PRESSURE_LOW :
            ((0) ? TEST_PRESSURE_MID : TEST_PRESSURE_NORMAL)));
/** ============== Count ============== */
define('TEST_MAX_CONCURRENCY', [1, 16, 32, 64, 256][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_CONCURRENCY_MID', [1, 8, 16, 32, 128][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_CONCURRENCY_LOW', [1, 4, 8, 16, 64][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_REQUESTS', [1, 16, 32, 64, 128][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_REQUESTS_MID', [1, 8, 16, 32, 64][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_REQUESTS_LOW', [1, 4, 8, 16, 32][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LENGTH_HIGH', [256, 256, 1024, 8192, 65535, 262144][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LENGTH', [64, 64, 256, 512, 1024][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LENGTH_LOW', [8, 8, 16, 32, 64][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LOOPS', (int) ([0.001, 1, 10, 100, 1000][TEST_PRESSURE_LEVEL] * 100));
define('TEST_MAX_PROCESSES', [1, 1, 2, 4, 8][TEST_PRESSURE_LEVEL]);

# ini

// TODO: remove it
ini_set('swow.better_error_info', true);

# functions

function getRandomBytes(int $length = 64): string
{
    if (function_exists('openssl_random_pseudo_bytes')) {
        $random = openssl_random_pseudo_bytes($length);
    } else {
        $random = random_bytes($length);
    }
    $random = base64_encode($random);
    $random = str_replace(['/', '+', '='], ['x', 'y', 'z'], $random);
    $random = substr($random, 0, $length);

    return $random;
}

function getRandomBytesArray(int $size, int $length = 64): array
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

# components

class Assert extends Swow\Assert
{
    // nothing, equal to alias
}
