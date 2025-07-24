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
/**
 * Author: Twosee <twose@qq.com>
 * Date: 2024/3/23 15:10
 */

namespace Swow\Debug\Debugger;

use SplFileObject;
use Swow\Buffer;

use function array_sum;
use function count;
use function explode;
use function extension_loaded;
use function is_array;
use function is_bool;
use function is_float;
use function is_int;
use function is_null;
use function is_object;
use function is_string;
use function max;
use function rtrim;
use function sprintf;
use function str_repeat;
use function strlen;
use function substr;

class DebuggerHelper
{
    public const SOURCE_FILE_CONTENT_PADDING = 4;
    public const SOURCE_FILE_DEFAULT_LINE_COUNT = 8;

    protected static ?bool $useMbString = null;

    protected static function hasMbString(): bool
    {
        if (static::$useMbString === null) {
            static::$useMbString = extension_loaded('mbstring');
        }

        return static::$useMbString;
    }

    protected static function strlen(string $string): int
    {
        if (!static::hasMbString()) {
            return strlen($string);
        }

        return mb_strlen($string);
    }

    protected static function substr(string $string, int $offset, ?int $length = null): string
    {
        if (!static::hasMbString()) {
            return substr($string, $offset, $length);
        }

        return mb_substr($string, $offset, $length);
    }

    protected static function getMaxLengthOfStringLine(string $string): int
    {
        $maxLength = 0;
        $lines = explode("\n", $string);
        foreach ($lines as $line) {
            $maxLength = max($maxLength, static::strlen($line));
        }

        return $maxLength;
    }

    /**
     * @param array<mixed> $table
     */
    public static function tableFormat(array $table): string
    {
        if (empty($table)) {
            return 'No more content';
        }
        $colLengthMap = [];
        foreach ($table as $item) {
            foreach ($item as $key => $value) {
                $key = (string) $key;
                $value = static::convertValueToString($value, false);
                $colLengthMap[$key] = max(
                    $colLengthMap[$key] ?? 0,
                    static::getMaxLengthOfStringLine($key),
                    static::getMaxLengthOfStringLine($value)
                );
            }
            unset($value);
        }
        // TODO: support \n in keys and values
        $width = array_sum($colLengthMap) + (count($colLengthMap) * 3) + 1;
        $bytes = (int) ($width * count($table) * 1.2);
        $buffer = new Buffer($bytes);
        $spiltLine = str_repeat('-', $width);
        $spiltBoldLine = str_repeat('=', $width);
        $buffer->append($spiltLine);
        $buffer->append("\n");
        $buffer->append('|');
        foreach ($colLengthMap as $key => $colLength) {
            $buffer->append(' ');
            $buffer->append(sprintf("%-{$colLength}s", $key));
            $buffer->append(' |');
        }
        $buffer->append("\n");
        $buffer->append($spiltBoldLine);
        $buffer->append("\n");
        foreach ($table as $item) {
            $buffer->append('|');
            foreach ($item as $key => $value) {
                $value = static::convertValueToString($value, false);
                $buffer->append(' ');
                $buffer->append(sprintf("%-{$colLengthMap[$key]}s", $value));
                $buffer->append(' |');
            }
            $buffer->append("\n");
        }
        $buffer->append($spiltLine);

        return $buffer->toString();
    }

    protected static function convertValueToString(mixed $value, bool $forArgs = true): string
    {
        switch (true) {
            case is_int($value):
            case is_float($value):
                return (string) $value;
            case is_null($value):
                return 'null';
            case is_bool($value):
                return $value ? 'true' : 'false';
            case is_string($value):
                {
                    // TODO: how to display binary content?
                    // if (!ctype_print($value)) {
                    //     $value = bin2hex($value);
                    // }
                    $maxLength = $forArgs ? 8 : 512;
                    if (static::strlen($value) > $maxLength) {
                        $value = static::substr($value, 0, $maxLength) . '...';
                    }
                    if ($forArgs) {
                        $value = "'{$value}'";
                    }

                    return $value;
                }
            case is_array($value):
                if (empty($value)) {
                    return '[]';
                }

                return '[...]';
            case is_object($value):
                return $value::class . '{}';
            default:
                return '...';
        }
    }

    /**
     * @param array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>|null,
     * } $frame
     */
    public static function convertFrameToExecutingString(array $frame): string
    {
        $atFunction = $frame['function'] ?? '';
        if ($atFunction) {
            $atClass = $frame['class'] ?? '';
            $delimiter = $atClass ? '::' : '';
            $args = $frame['args'] ?? [];
            $argsString = '';
            if ($args) {
                foreach ($args as $argValue) {
                    $argsString .= static::convertValueToString($argValue) . ', ';
                }
                $argsString = rtrim($argsString, ', ');
            }
            $executing = "{$atClass}{$delimiter}{$atFunction}({$argsString})";
        } else {
            $executing = 'Unknown';
        }

        return $executing;
    }

    /**
     * @param array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>|null,
     *     'file': string|null,
     *     'line': int|null,
     * }> $trace
     * @return array<array{
     *     'frame': int,
     *     'executing': string,
     *     'source_position': string,
     * }>
     */
    public static function convertTraceToTable(array $trace, ?int $frameIndex = null): array
    {
        $traceTable = [];
        foreach ($trace as $index => $frame) {
            if ($frameIndex !== null && $index !== $frameIndex) {
                continue;
            }
            $executing = static::convertFrameToExecutingString($frame);
            $file = $frame['file'] ?? null;
            $line = $frame['line'] ?? '?';
            if ($file === null) {
                $sourcePosition = '<internal space>';
            } else {
                $sourcePosition = "{$file}({$line})";
            }
            $traceTable[] = [
                'frame' => $index,
                'executing' => $executing,
                'source_position' => $sourcePosition,
            ];
            if ($frameIndex !== null) {
                break;
            }
        }
        if (empty($traceTable)) {
            throw new DebuggerException('No trace info');
        }

        return $traceTable;
    }

    /**
     * @return array<array{
     *     'line': string|int,
     *     'content': string,
     * }>
     * @phpstan-return array<array{
     *     'line': string|positive-int,
     *     'content': string,
     * }>
     * @psalm-return array<array{
     *     'line': string|positive-int,
     *     'content': string,
     * }>
     */
    public static function getSourceFileContentAsTable(
        string $filename,
        int $line,
        ?SplFileObject &$sourceFile = null,
        int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT,
    ): array {
        $sourceFile = null;
        if ($line < 2) {
            $startLine = $line;
        } else {
            $startLine = $line - ($lineCount - static::SOURCE_FILE_CONTENT_PADDING - 1);
        }
        $file = new SplFileObject($filename);
        $sourceFile = $file;
        $i = 0;
        while (!$file->eof()) {
            $lineContent = $file->fgets();
            $i++;
            if ($i === $startLine - 1) {
                break;
            }
        }
        if (!isset($lineContent)) {
            throw new DebuggerException('File Line not found');
        }
        $contents = [];
        for ($i++; $i < $startLine + $lineCount; $i++) {
            if ($file->eof()) {
                break;
            }
            $lineContent = $file->fgets();
            $contents[] = [
                'line' => $i === $line ? "{$i}->" : $i,
                'contents' => rtrim($lineContent),
            ];
        }

        return $contents;
    }

    /**
     * @return array<array{
     *     'line': int,
     *     'content': string,
     * }>
     */
    public static function getFollowingSourceFileContent(
        SplFileObject $sourceFile,
        int $startLine,
        int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT,
        int $offset = self::SOURCE_FILE_CONTENT_PADDING,
    ): array {
        $contents = [];
        for ($i = $startLine + $offset + 1; $i < $startLine + $offset + $lineCount; $i++) {
            if ($sourceFile->eof()) {
                break;
            }
            $contents[] = [
                'line' => $i,
                'contents' => rtrim($sourceFile->fgets()),
            ];
        }

        return $contents;
    }
}
