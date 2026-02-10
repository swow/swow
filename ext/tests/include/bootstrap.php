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

# env constants
define('USE_VALGRIND', getenv('USE_ZEND_ALLOC') === '0');
define('TEST_POSTGRES_HOST', getenv('POSTGRES_HOST') ?: '127.0.0.1');
define('TEST_POSTGRES_PORT', getenv('POSTGRES_PORT') ?: 5432);
define('TEST_POSTGRES_USER', getenv('POSTGRES_USER') ?: 'postgres');
define('TEST_POSTGRES_PASSWORD', getenv('POSTGRES_PASSWORD') ?: 'postgres');
define('TEST_POSTGRES_DBNAME', getenv('POSTGRES_DBNAME') ?: 'postgres');

# pressure constants
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

# count constants
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

# website constants
define('TEST_WEBSITE1_URL', getenv('GITHUB_ACTIONS') ? 'https://github.com/' : 'https://www.apple.com/');
define('TEST_WEBSITE1_KEYWORD', getenv('GITHUB_ACTIONS') ? 'GitHub' : 'apple');
define('TEST_WEBSITE2_URL', getenv('GITHUB_ACTIONS') ? 'https://www.bing.com/' : 'https://www.baidu.com/');
define('TEST_WEBSITE2_KEYWORD', getenv('GITHUB_ACTIONS') ? 'Bing' : 'baidu');

# ini
if (extension_loaded(Swow::class) && ini_get('swow.enable')) {
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
        PHP_OS_FAMILY !== 'Windows' ? '/tmp/swow_test_%s.sock' : '\\\?\pipe\swow_test_%s',
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

/**
 * split posix args into array, like python shlex.split
 *
 * @param string $args the posix args string to split
 * @return array the splitted args
 */
function shell_split(string $args): array
{
    $ret = [];
    $len = strlen($args);
    $i = 0;
    $in_quote = false;
    $quote = '';
    $arg = '';
    while ($i < $len) {
        $c = $args[$i];
        if ($in_quote) {
            if ($c === $quote) {
                $in_quote = false;
            } else {
                $arg .= $c;
            }
        } else {
            if ($c === '"' || $c === "'") {
                $in_quote = true;
                $quote = $c;
            } elseif ($c === ' ') {
                if ($arg !== '') {
                    $ret[] = $arg;
                    $arg = '';
                }
            } else {
                $arg .= $c;
            }
        }
        $i++;
    }
    if ($arg !== '') {
        $ret[] = $arg;
    }
    return $ret;
}

/**
 * Returns testing php absulote path
 *
 * @return string the testing php absulote path
 */
function test_php_path(): string
{
    return realpath(getenv('TEST_PHP_EXECUTABLE') ?: PHP_BINARY);
}

/**
 * Returns php options that enables swow
 *
 * @return array args that enables swow
 */
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
            // installed
            ['-d', 'extension=swow'],
            // made in phpize
            ['-d', 'extension=.libs/swow' . (PHP_OS_FAMILY === 'Darwin' ? '.dylib' : '.so')],
            // made in-tree
            ['-d', 'extension=modules/swow' . (PHP_OS_FAMILY === 'Darwin' ? '.dylib' : '.so')],
        ],
    };
    // run-test args
    array_unshift($try_args, [
        ...shell_split(getenv('TEST_PHP_ARGS') ?: ''),
        ...shell_split(getenv('TEST_PHP_EXTRA_ARGS') ?: ''),
    ]);
    // installed, enabled
    array_unshift($try_args, []);

    $ext_args = null;
    foreach ($try_args as $args) {
        $is_swow_enabled = shell_exec(test_php_path() . ' --ri swow ' . implode(' ', $args) . ' 2>&1');
        if (!str_contains($is_swow_enabled, 'not present')) {
            $ext_args = $args;
            break;
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
    return shell_exec(test_php_path() . ' ' . implode(' ', php_options_with_swow()) . ' ' . $args . ' 2>&1');
}

function php_proc_with_swow(array $args, callable $handler, array $options = []): int
{
    $_args = array_merge([test_php_path()], php_options_with_swow(), $args);
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

/**
 * @param array<string, string> $headers
 * @return array<array{
 *     'status': int,
 *     'headers': array<string, string>,
 *     'body': string,
 * }>
 */
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
    if (PHP_VERSION_ID >= 80400) {
        $http_response_header = http_get_last_response_headers();
    }
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

function make_fifo(): ?string
{
    $path = sprintf(
        PHP_OS_FAMILY !== 'Windows' ? '/tmp/swow_test_%s.fifo' : '\\\.\pipe\swow_test_%s',
        getRandomBytes(8)
    );

    if (PHP_OS_FAMILY !== 'Windows') {
        shell_exec("mkfifo \"{$path}\"");
        $stat = @stat($path);
        if ($stat === false || !($stat['mode'] | 0x1000)) {
            // mkfifo failed, try mknod
            @unlink($path);
            shell_exec("mknod \"{$path}\" p");
            $stat = @stat($path);
            if ($stat === false || !($stat['mode'] | 0x1000)) {
                @unlink($path);
                return null;
            }
        }
    } else {
        // new Socket(Socket::TYPE_PIPE) cannot be used
        // because dwOpenMode FILE_FLAG_FIRST_PIPE_INSTANCE is specified
        // the flag will make following opens fail with ERROR_ACCESS_DENIED
        if (!extension_loaded('ffi')) {
            // no way to create fifo on windows
            return null;
        }
        try {
            // ffi may not be enabled
            $ffi = FFI::cdef('
                void *CreateNamedPipeA(
                    const char *lpName,
                    int dwOpenMode,
                    int dwPipeMode,
                    int nMaxInstances,
                    int nOutBufferSize,
                    int nInBufferSize,
                    int nDefaultTimeOut,
                    const void *lpSecurityAttributes
                );
                void CloseHandle(void *hObject);
            ', 'kernel32.dll');
            $handle = $ffi->CreateNamedPipeA(
                $path,
                0x00000003 /* PIPE_ACCESS_DUPLEX */,
                0 /* PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_ACCEPT_REMOTE_CLIENTS */,
                255 /* PIPE_UNLIMITED_INSTANCES */,
                0, // no buffer
                0, // no buffer
                0, // default time out
                null, // no security specified
            );
        } catch (Throwable $e) {
            var_dump($e);
            return null;
        }
    }
    return $path;
}

/**
 * for tests on PHP without openssl extension
 */
function testX509PEMs(): array
{
    $serializedBase64ed = <<<'TEXT'
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
TEXT;

    $unserialized = unserialize(base64_decode($serializedBase64ed, true));
    $pems = [];
    foreach ($unserialized as $name => $pair) {
        $certBin = $pair['cert'];
        $keyBin = $pair['key'];
        $cert = "-----BEGIN CERTIFICATE-----\n" . implode("\n", str_split(base64_encode($certBin), 64)) . "\n-----END CERTIFICATE-----\n"; // gitleaks:allow
        $key = "-----BEGIN EC PRIVATE KEY-----\n" . implode("\n", str_split(base64_encode($keyBin), 64)) . "\n-----END EC PRIVATE KEY-----\n"; // gitleaks:allow
        $pems[$name] = [
            'cert' => $cert,
            'key' => $key,
        ];
    }
    return $pems;
}

function testX509Paths(string $dirname): array
{
    @mkdir($dirname, 0755, true);
    $pems = testX509PEMs();
    foreach ($pems as $name => $pem) {
        $paths[$name] = [
            'cert' => $dirname . '/' . $name . '.crt',
            'key' => $dirname . '/' . $name . '.key',
        ];
        file_put_contents($paths[$name]['cert'], $pem['cert']);
        file_put_contents($paths[$name]['key'], $pem['key']);
    }

    $verydeepCertPath = $paths['verydeep']['cert'];
    $verydeepCertFile = fopen($verydeepCertPath, 'ab');
    foreach ($paths as $certName => $path) {
        if (!str_contains($certName, 'intermediate')) {
            continue;
        }
        fwrite($verydeepCertFile, file_get_contents($path['cert']));
    }
    fclose($verydeepCertFile);

    return $paths;
}

function rmtree(string $dirname): void
{
    foreach (scandir($dirname) as $file) {
        if ($file === '.' || $file === '..') {
            continue;
        }
        unlink($dirname . '/' . $file);
    }
    rmdir($dirname);
}
