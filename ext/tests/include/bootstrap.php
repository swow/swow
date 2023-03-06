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

require __DIR__ . '/autoload.php';

# PHP ini

ini_set('display_errors', '1');
ini_set('display_startup_errors', '1');
ini_set('report_memleaks', '1');
ini_set('memory_limit', '64M');

error_reporting(E_ALL);

# constants

/* ============== Env =============== */
define('USE_VALGRIND', getenv('USE_ZEND_ALLOC') === '0');
define('TEST_POSTGRES_HOST', getenv('POSTGRES_HOST') ?: '127.0.0.1');
define('TEST_POSTGRES_PORT', getenv('POSTGRES_PORT') ?: 5432);
define('TEST_POSTGRES_USER', getenv('POSTGRES_USER') ?: 'postgres');
define('TEST_POSTGRES_PASSWORD', getenv('POSTGRES_PASSWORD') ?: 'postgres');
define('TEST_POSTGRES_DBNAME', getenv('POSTGRES_DBNAME') ?: 'postgres');
/* ============== Pressure ============== */
define('TEST_PRESSURE_DEBUG', 0);
define('TEST_PRESSURE_MIN', 1);
define('TEST_PRESSURE_LOW', 2);
define('TEST_PRESSURE_MID', 3);
define('TEST_PRESSURE_NORMAL', 4);
define(
    'TEST_PRESSURE_LEVEL',
    (getenv('TEST_PRESSURE_DEBUG') === '1') ? TEST_PRESSURE_DEBUG :
        (USE_VALGRIND ? TEST_PRESSURE_LOW :
            ((0) ? TEST_PRESSURE_MID : TEST_PRESSURE_NORMAL))
);
/* ============== Count ============== */
define('TEST_MAX_CONCURRENCY', [1, 8, 16, 32, 64][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_CONCURRENCY_MID', [1, 4, 8, 16, 32][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_CONCURRENCY_LOW', [1, 2, 4, 8, 16][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_REQUESTS', [1, 16, 32, 64, 128][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_REQUESTS_MID', [1, 8, 16, 32, 64][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_REQUESTS_LOW', [1, 4, 8, 16, 32][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LENGTH_HIGH', [256, 256, 1024, 8192, 65535, 262144][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LENGTH', [64, 64, 256, 512, 1024][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LENGTH_LOW', [8, 8, 16, 32, 64][TEST_PRESSURE_LEVEL]);
define('TEST_MAX_LOOPS', (int) ([0.001, 1, 10, 100, 1000][TEST_PRESSURE_LEVEL] * 100));
define('TEST_MAX_PROCESSES', [1, 1, 2, 4, 8][TEST_PRESSURE_LEVEL]);

# ini
if (extension_loaded(Swow::class)) {
    Swow\Socket::setGlobalTimeout(15 * 1000);
}

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

    return substr($random, 0, $length);
}

function getRandomBytesArray(int $size, int $length = 64): array
{
    $randoms = [];
    while ($size--) {
        $randoms[] = getRandomBytes($length);
    }

    return $randoms;
}

function getRandomPipePath(): string
{
    return sprintf(
        PHP_OS_FAMILY !== 'Windows' ? '/tmp/swow_test_%s.sock' : '\\\\?\\pipe\\swow_test_%s',
        getRandomBytes(8)
    );
}

function switchProcess(): void
{
    usleep(100 * 1000);
}

function isInTest(): bool
{
    global $argv;

    return !str_ends_with($argv[0], '.phpt');
}

function phpt_sprint(...$args): void
{
    if (isInTest()) {
        return;
    }
    echo sprintf(...$args);
}

function phpt_var_dump(...$args): void
{
    if (isInTest()) {
        return;
    }
    var_dump(...$args);
}

function pseudo_random_sleep(): void
{
    static $seed = 42;
    // simple LCG with fixed seed
    $seed = (75 * $seed + 74) % 65537;
    usleep($seed);
}

function real_php_path(): string
{
    return realpath(PHP_BINARY);
}

function php_options_with_swow(): array
{
    static $options;
    if ($options) {
        return $options;
    }

    // these options are from run-tests.php for running child process with same options
    $options = [
        '-d', 'error_prepend_string=',
        '-d', 'error_append_string=',
        '-d', 'error_reporting=32767',
        '-d', 'display_errors=1',
        '-d', 'display_startup_errors=1',
        '-d', 'log_errors=0',
        '-d', 'html_errors=0',
        '-d', 'track_errors=0',
    ];

    // guessing
    $try_args = match (PHP_OS_FAMILY) {
        'Windows' => [
            // installed, enabled
            [],
            // installed
            ['-d', 'extension=swow'],
            // made
            [
                '-d',
                'extension_dir=x64\\' .
                    (PHP_DEBUG ? 'Debug' : 'Release') .
                    (PHP_ZTS ? '' : '_TS'),
                '-d',
                'extension=swow',
            ],
        ],
        default => [
            // installed, enabled
            [],
            // installed
            ['-d', 'extension=swow'],
            // made in phpize
            ['-d', 'extension=.libs/swow' . (PHP_OS_FAMILY === 'Darwin' ? '.dylib' : '.so')],
            // made in-tree
            ['-d', 'extension=modules/swow' . (PHP_OS_FAMILY === 'Darwin' ? '.dylib' : '.so')],
        ]
    };

    $ext_args = null;
    foreach ($try_args as $args) {
        $is_swow_enabled = shell_exec(real_php_path() . ' --ri swow ' . implode(' ', $args) . ' 2>&1');
        if (!str_contains($is_swow_enabled, 'not present')) {
            $ext_args = $args;
        }
    }
    if ($ext_args === null) {
        throw new Exception('cannot find installed swow, have you built/installed swow?');
    }

    $options = [...$options, ...$ext_args];

    return $options;
}

function php_exec_with_swow(string $args)
{
    return shell_exec(real_php_path() . ' ' . implode(' ', php_options_with_swow()) . ' ' . $args . ' 2>&1');
}

function php_proc_with_swow(array $args, callable $handler, array $options = []): int
{
    $_args = array_merge([real_php_path()], php_options_with_swow(), $args);
    $proc = proc_open($_args, [
        0 => $options['stdin'] ?? STDIN,
        1 => $options['stdout'] ?? ['pipe', 'w'],
        2 => $options['stderr'] ?? ['pipe', 'w'],
    ], $pipes, null, null);
    $handler($proc, $pipes);
    return proc_close($proc);
}

function getHttpProxyUri(): string
{
    $proxy = str_replace('http://', 'tcp://', getenv('https_proxy') ?: getenv('http_proxy') ?: getenv('all_proxy') ?: '', $count);
    if ($proxy && $count === 0) {
        $proxy = "tcp://{$proxy}";
    }
    return $proxy;
}

/** @return array<array{'status': int, 'headers': array<string, string>, 'body': string}> */
function httpRequest(string $url, string $method = 'GET', string $content = '', array $headers = [], ?int $timeoutSeconds = null, bool|string $proxy = false, bool $doNotThrow = false): array|false
{
    $headers += [
        'User-Agent' => 'curl',
        'Accept' => '*/*',
    ];
    $headerLines = [];
    foreach ($headers as $name => $value) {
        $headerLines[] = "{$name}: {$value}";
    }
    $header = implode("\r\n", $headerLines);
    if ($proxy === true) {
        $proxy = getHttpProxyUri();
    }
    $httpContext = [
        'method' => $method,
        'header' => $header,
        'request_fulluri' => true,
    ];
    if ($content) {
        $httpContext['content'] = $content;
    }
    if ($timeoutSeconds !== null) {
        $httpContext['timeout'] = $timeoutSeconds;
    }
    if ($proxy) {
        $httpContext['proxy'] = $proxy;
    }
    $content = @file_get_contents(
        filename: $url,
        context: stream_context_create([
            'http' => $httpContext,
        ])
    );
    if (!$content && empty($http_response_header)) {
        if ($doNotThrow) {
            return false;
        }
        throw new RuntimeException(sprintf('Failed to download from %s (%s)', $url, error_get_last()['message']));
    }
    $status = 0;
    $headers = [];
    foreach ($http_response_header as $headerLine) {
        $parts = explode(':', $headerLine, 2);
        if (count($parts) === 1) {
            $status = explode(' ', trim($parts[0]), 3)[1];
        } else {
            $headers[strtolower(trim($parts[0]))] = trim($parts[1]);
        }
    }
    return [
        'status' => (int) $status,
        'headers' => $headers,
        'body' => $content ?: '',
    ];
}
