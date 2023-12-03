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

namespace Swow\Debug;

use Closure;
use Stringable;
use Swow\Coroutine;
use Swow\Debug\Debugger\Debugger;
use Swow\Utils\FileSystem\IOException;
use Swow\Utils\Handler;
use TypeError;

use function debug_backtrace;
use function fclose;
use function fgets;
use function fopen;
use function fwrite;
use function is_array;
use function is_callable;
use function is_scalar;
use function is_string;
use function ob_get_clean;
use function ob_start;
use function rtrim;
use function sprintf;
use function var_dump;

use const DEBUG_BACKTRACE_IGNORE_ARGS;
use const STDERR;

function isStringable(mixed $value): bool
{
    return $value === null || is_scalar($value) || $value instanceof Stringable;
}

function isStrictStringable(mixed $value): bool
{
    return is_string($value) || $value instanceof Stringable;
}

function var_dump_return(mixed ...$expressions): string
{
    ob_start();

    var_dump(...$expressions);

    return ob_get_clean();
}

function registerDeadlockDetectorHandler(): Handler
{
    return Coroutine::registerDeadlockHandler(static function (): void {
        fwrite(STDERR, 'Deadlock: all coroutines are asleep');
        Debugger::runOnTTY();
    });
}

function showExecutedSourceLines(mixed $filter = false, bool $force = false): void
{
    static $registeredHandler;
    static $getFileLineCached;
    static $showAllHandler;
    $getFileLineCached ??= static function (string $file, int $line): string {
        // TODO: LRU cache
        static $cache = [];
        if (!isset($cache[$file])) {
            $fileLines = [];
            $fp = @fopen($file, 'rb');
            if ($fp === false) {
                $cache[$file] = false;
            } else {
                while (true) {
                    $lineContent = fgets($fp);
                    if ($lineContent === false) {
                        break;
                    }
                    $fileLines[] = rtrim($lineContent);
                }
                $cache[$file] = $fileLines;
            }
        }
        if (!$cache[$file]) {
            return "Unable to read file {$file}";
        }
        if (!isset($cache[$file][$line - 1])) {
            return "Unable to read file line {$file}:{$line}";
        }

        return $cache[$file][$line - 1];
    };
    if ($filter === true) {
        $handler = ($showAllHandler ??= static function () use ($getFileLineCached): void {
            static $lastFile = '', $firstTouch = false;
            if (!$firstTouch) {
                /* skip for this function ending */
                $firstTouch = true;
                return;
            }
            $call = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0];
            $file = $call['file'] ?? null;
            $line = $call['line'] ?? null;
            if ($lastFile !== $file) {
                $lastFile = $file;
                echo ">>> {$file}\n";
            }
            echo sprintf("%-6s%s\n", $line, $getFileLineCached($file, $line));
        });
    } elseif ($filter === false) {
        $file = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0]['file'] ?? '';
        $fp = @fopen($file, 'rb');
        if ($fp === false) {
            throw IOException::createFromLastError();
        }
        $lines = [];
        while (($line = fgets($fp)) !== false) {
            $lines[] = rtrim($line);
        }
        fclose($fp);
        $handler = static function () use ($file, $lines): void {
            $call = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0];
            $callFile = $call['file'];
            $callLine = $call['line'];
            if ($callFile === $file) {
                echo sprintf("%-6s%s\n", $callLine, $lines[$callLine - 1]);
            }
        };
    } elseif (is_callable($filter)) {
        $filter = Closure::fromCallable($filter);
        $handler = static function () use ($filter, $getFileLineCached): void {
            static $lastFile = '';
            $call = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0];
            $file = $call['file'] ?? null;
            $line = $call['line'] ?? null;
            if ($filter($file, $line)) {
                if ($lastFile !== $file) {
                    $lastFile = $file;
                    echo ">>> {$file}\n";
                }
                echo sprintf("%-6s%s\n", $line, $getFileLineCached($file, $line));
            }
        };
    } elseif (is_array($filter)) {
        $filter = array_flip($filter);
        $handler = static function () use ($filter, $getFileLineCached): void {
            static $lastFile = '';
            $call = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0];
            $file = $call['file'] ?? null;
            $line = $call['line'] ?? null;
            if (isset($filter[$file])) {
                if ($lastFile !== $file) {
                    $lastFile = $file;
                    echo ">>> {$file}\n";
                }
                echo sprintf("%-6s%s\n", $line, $getFileLineCached($file, $line));
            }
        };
    } elseif (is_string($filter)) {
        $isRegex = ($filter[0] === '/' && $filter[-1] === '/') || ($filter[0] === '#' && $filter[-1] === '#');
        $handler = static function () use ($filter, $isRegex, $getFileLineCached): void {
            static $lastFile = '';
            $call = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0];
            $file = $call['file'] ?? null;
            $line = $call['line'] ?? null;
            if ($isRegex ? preg_match($filter, $file) : ($file === $filter)) {
                if ($lastFile !== $file) {
                    $lastFile = $file;
                    echo ">>> {$file}\n";
                }
                echo sprintf("%-6s%s\n", $line, $getFileLineCached($file, $line));
            }
        };
    } elseif ($filter === null) {
        $registeredHandler->remove();
    } else {
        throw new TypeError(sprintf('%s(): Argument #1 ($filter) must be of type bool, callable or null, %s given', __FUNCTION__, get_debug_type($filter)));
    }
    if (isset($handler)) {
        $registeredHandler?->remove();
        $registeredHandler = registerExtendedStatementHandler($handler, $force);
    }
}
