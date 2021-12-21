<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

namespace Swow\Tools;

use function fgets;
use function trim;

require_once __DIR__ . '/autoload.php';

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
    return ($color !== COLOR_NONE ? "\033[3{$color}m{$string}\033[0m" : $string);
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
const EMOJI_NOTICE = '💬';
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
        } elseif ($response === 'n') {
            return false;
        }
    }
}

function passthru(...$commands): int
{
    $command = implode(" && \\\n", $commands);
    log("> {$command}");
    \passthru($command, $status);

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

function executeAndCheck(array $commands): void
{
    $basename = pathinfo($commands[1] ?? '', PATHINFO_FILENAME);
    echo "[{$basename}]" . PHP_EOL;
    echo "===========  Execute  ==============" . PHP_EOL;
    $command = implode(' ', $commands);
    exec($command, $output, $return_var);
    if (str_starts_with($output[0] ?? '', '#!')) {
        array_shift($output);
    }
    echo '> ' . implode("\n> ", $output) . "" . PHP_EOL;
    if ($return_var != 0) {
        error("Exec {$command} failed with code {$return_var}!");
    }
    echo "=========== Finish Done ============" . PHP_EOL . PHP_EOL;
}

function fileSize(string $filename, int $decimals = 2): string
{
    $bytes = filesize($filename);
    $units = ['B', 'K', 'M', 'G', 'T', 'P'];
    $factor = (int) floor((strlen($bytes) - 1) / 3);
    return sprintf("%.{$decimals}f", $bytes / 1024 ** $factor) . $units[$factor];
}

function gitFiles(string $root = '.'): array
{
    return explode(PHP_EOL, `cd {$root} && git ls-files`);
}

function defer(&$any, callable $callback)
{
    if (!$any) {
        $any = new class {
            private array $callbacks = [];

            public function add(callable $callback)
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
