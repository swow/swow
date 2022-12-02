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

namespace Swow\Utils;

use RuntimeException;
use Swow;

use function array_shift;
use function error_get_last;
use function feof;
use function fgets;
use function file_exists;
use function file_get_contents;
use function file_put_contents;
use function filesize;
use function fread;
use function getenv;
use function implode;
use function is_numeric;
use function md5;
use function passthru as native_passthru;
use function proc_close;
use function proc_get_status;
use function proc_open;
use function sprintf;
use function str_replace;
use function stream_context_create;
use function strtolower;
use function sys_get_temp_dir;
use function trigger_error;
use function trim;

use const E_USER_WARNING;
use const PATHINFO_FILENAME;
use const PHP_EOL;
use const STDIN;

const COLOR_NONE = 0;
const COLOR_RED = 1;
const COLOR_GREEN = 2;
const COLOR_YELLOW = 3;
const COLOR_BLUE = 4;
const COLOR_MAGENTA = 5;
const COLOR_CYAN = 6;
const COLOR_WHITE = 7;

function dye(string $string, int $color): string
{
    return $color !== COLOR_NONE ? "\033[3{$color}m{$string}\033[0m" : $string;
}

function log(string $message, int $color = COLOR_NONE): void
{
    echo dye($message, $color) . PHP_EOL;
}

function br(): void
{
    echo PHP_EOL;
}

const EMOJI_WARN = '⚠️';
const EMOJI_ERROR = '❌';
const EMOJI_NOTICE = '🔎';
const EMOJI_INFO = '⛄️';
const EMOJI_DEBUG = '🧬';
const EMOJI_OK = '✅';
const EMOJI_SUCCESS = '🚀';

function warn(string $message): void
{
    log(EMOJI_WARN . " {$message}", COLOR_YELLOW);
}

function error(string $message): void
{
    log(EMOJI_ERROR . " {$message}", COLOR_RED);
    exit(255);
}

function notice(string $message): void
{
    log(EMOJI_NOTICE . " {$message}", COLOR_CYAN);
}

function info(string $message): void
{
    log(EMOJI_INFO . " {$message}", COLOR_NONE);
}

function debug(string $message, int $level = 1): void
{
    static $debugLevel;
    if (!isset($debugLevel)) {
        $debugLevel = getenv('DEBUG');
        if (is_numeric($debugLevel)) {
            $debugLevel = $debugLevel + 0;
        } else {
            $debugLevel = 0;
        }
    }
    if ($debugLevel >= $level) {
        log(EMOJI_DEBUG . " {$message}", COLOR_MAGENTA);
    }
}

function ok(string $message): void
{
    log(EMOJI_OK . " {$message}", COLOR_GREEN);
}

function success(string $content): void
{
    $em = str_repeat(EMOJI_SUCCESS, 3);
    log("{$em} {$content} {$em}", COLOR_CYAN);
    exit(0);
}

function check(bool $is_ok, string $message): void
{
    if ($is_ok) {
        ok("{$message} OK!");
    } else {
        error("{$message} Failed!");
    }
}

function askBool(string $question): bool
{
    while (true) {
        echo dye('👀' . " {$question} (Y/n): ", COLOR_YELLOW);
        $response = trim(fgets(STDIN));
        if ($response === 'Y') {
            return true;
        }
        if ($response === 'n') {
            return false;
        }
    }
}

function passthru(string ...$commands): int
{
    $command = implode(" && \\\n", $commands);
    log("> {$command}");
    native_passthru($command, $status);

    return $status;
}

function spaces(int $length): string
{
    return str_repeat(' ', $length);
}

function camelize(string $uncamelized_words, string $separator = '_'): string
{
    $uncamelized_words = $separator . str_replace($separator, ' ', strtolower($uncamelized_words));
    return ltrim(str_replace(' ', '', ucwords($uncamelized_words)), $separator);
}

function unCamelize(string $camelCaps, string $separator = '_'): string
{
    $camelCaps = preg_replace('/([a-z])([A-Z])/', "\${1}{$separator}\${2}", $camelCaps);
    /* for special case like: PDOPool => pdo_pool */
    $camelCaps = preg_replace('/([A-Z]+)([A-Z][a-z]+)/', "\${1}{$separator}\${2}\${3}", $camelCaps);
    return strtolower($camelCaps);
}

function printAsMultiLines(string $title = '', int $length = 32): void
{
    if ($length % 2 !== 0) {
        $length += 1;
    }
    echo "< {$title} > " . str_repeat('=', $length) . PHP_EOL;
}

/**
 * execute command and check command result
 *
 * @param array<string> $commands
 */
function executeAndCheck(array $commands): void
{
    $basename = pathinfo($commands[1] ?? '', PATHINFO_FILENAME);
    echo "[{$basename}]" . PHP_EOL;
    echo '===========  Execute  ==============' . PHP_EOL;
    $command = implode(' ', $commands);
    exec($command, $output, $return_var);
    if (str_starts_with($output[0] ?? '', '#!')) {
        array_shift($output);
    }
    echo '> ' . implode("\n> ", $output) . '' . PHP_EOL;
    if ($return_var !== 0) {
        error("Exec {$command} failed with code {$return_var}!");
    }
    echo '=========== Finish Done ============' . PHP_EOL . PHP_EOL;
}

function formatSize(int $size, $precision = 2)
{
    $units = ['B', 'KB', 'MB', 'GB', 'TB'];
    $level = 0;
    while ($size > 1024) {
        $size /= 1024;
        $level++;
    }
    return round($size, $precision) . ' ' . $units[$level];
}

function fileFormattedSize(string $filename, int $precision = 2): string
{
    return formatSize(filesize($filename), $precision);
}

/**
 * @return array<string>
 */
function gitFiles(string $root = '.'): array
{
    if (is_dir("{$root}/.git")) {
        return explode(PHP_EOL, shell_exec("git --git-dir={$root}/.git ls-files"));
    }

    return explode(PHP_EOL, shell_exec("git --git-dir={$root} ls-files"));
}

/**
 * defer execute
 *
 * @param-out \stdClass $any reference to any variable used for defer
 */
function defer(mixed &$any, callable $callback): void
{
    if (!$any) {
        $any = new class() {
            /** @var array<callable> */
            private array $callbacks = [];

            public function add(callable $callback): void
            {
                $this->callbacks[] = $callback;
            }

            public function __destruct()
            {
                $callbacks = array_reverse($this->callbacks);
                foreach ($callbacks as $callback) {
                    $callback();
                }
            }
        };
    }
    $any->add($callback);
}

/**
 * @param array<string> $command
 * @param array{
 *              'command': string,
 *              'pid': int,
 *              'running': bool,
 *              'signaled':bool,
 *              'stopped': bool,
 *              'exitcode':int,
 *              'termsig':int,
 *              'stopsig': int
 *        }|false $status
 */
function processExecute(array $command, &$status = null): string
{
    $proc = proc_open(
        $command,
        [0 => STDIN, 1 => ['pipe', 'w'], 2 => ['redirect', 1]],
        $pipes,
        null,
        null
    );
    $output = '';
    do {
        $output .= fread($pipes[1], 8192);
    } while (!feof($pipes[1]));
    $status = proc_get_status($proc);
    proc_close($proc);

    return $output;
}

function httpGet(string $url): string
{
    $requestArgs = ['filename' => $url];
    if (getenv('https_proxy') || getenv('http_proxy') || getenv('all_proxy')) {
        $requestArgs['context'] = stream_context_create([
            'http' => [
                'proxy' => str_replace('http://', 'tcp://', getenv('https_proxy') ?: getenv('http_proxy') ?: getenv('all_proxy')),
                'request_fulluri' => true,
            ],
        ]);
    }
    $response = @file_get_contents(...$requestArgs);
    if (!$response) {
        throw new RuntimeException(sprintf('Failed to download from %s (%s)', $url, error_get_last()['message']));
    }

    return $response;
}

function httpGetCached(string $url): string
{
    $tmpFilePath = sprintf('%s/%s_%s_%s', sys_get_temp_dir(), strtolower(Swow::class), 'http_get_cache', md5($url));
    if (file_exists($tmpFilePath)) {
        $contents = @file_get_contents($tmpFilePath);
        if ($contents !== false) {
            return $contents;
        }
        trigger_error(sprintf('Failed to load cache from %s (%s)', $tmpFilePath, error_get_last()['message']), E_USER_WARNING);
    }
    $contents = httpGet($url);
    if (!@file_put_contents($tmpFilePath, $contents)) {
        trigger_error(sprintf('Failed to cache HTTP response to %s (%s)', $tmpFilePath, error_get_last()['message']), E_USER_WARNING);
    }
    return $contents;
}

function httpDownload(string $from, string $to): void
{
    $contents = httpGet($from);
    if (!@file_put_contents($to, $contents)) {
        throw new RuntimeException(sprintf('Failed to put content to %s (%s)', $to, error_get_last()['message']));
    }
}
