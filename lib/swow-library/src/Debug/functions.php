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

use Swow\Coroutine;
use Swow\Util\FileSystem\IOException;
use Swow\Util\Handler;
use function debug_backtrace;
use function fclose;
use function fgets;
use function fopen;
use function fwrite;
use function ob_get_clean;
use function ob_start;
use function rtrim;
use function sprintf;
use function var_dump;
use const DEBUG_BACKTRACE_IGNORE_ARGS;
use const PHP_EOL;
use const STDERR;

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

function showExecutedSourceLines(bool $all = false): Handler
{
    $getFileLineCached = static function (string $file, int $line): string {
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
    if ($all) {
        $handler = static function () use ($getFileLineCached): void {
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
                echo '>>> ' . $file . PHP_EOL;
            }
            echo sprintf('%-6s%s' . PHP_EOL, $line, $getFileLineCached($file, $line));
        };
    } else {
        $file = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0]['file'] ?? '';
        $fp = @fopen($file, 'rb');
        if ($fp === false) {
            throw IOException::getLast();
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
                echo sprintf('%-6s%s' . PHP_EOL, $callLine, $lines[$callLine - 1]);
            }
        };
    }

    return registerExtendedStatementHandler($handler);
}
