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
 * Date: 2024/3/24 03:45
 */

namespace Swow\Debug\Debugger;

use SplFileObject;
use Swow\Coroutine;

use function basename;
use function file_exists;
use function is_a;
use function is_numeric;

trait DebuggerStaticGetterTrait
{
    protected static function getStateNameOfCoroutine(Coroutine $coroutine): string
    {
        if (static::getDebugContextOfCoroutine($coroutine)->stopped) {
            $state = 'stopped';
        } else {
            $state = $coroutine->getStateName();
        }

        return $state;
    }

    protected static function getExtendedLevelOfCoroutineForTrace(Coroutine $coroutine): int
    {
        if (static::getDebugContextOfCoroutine($coroutine)->stopped) {
            // skip extended statement handler
            $level = static::getCoroutineTraceDiffLevel($coroutine, __FUNCTION__);
        } else {
            $level = 0;
        }

        return $level;
    }

    protected static function getExtendedLevelOfCoroutineForExecution(Coroutine $coroutine): int
    {
        return static::getExtendedLevelOfCoroutineForTrace($coroutine) + 1;
    }

    /**
     * @return array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * }>|array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * } $trace
     * @psalm-return ($index is null ? array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * }> : array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * }) $trace
     */
    public static function getTraceOfCoroutine(Coroutine $coroutine, ?int $index = null): array
    {
        $level = static::getExtendedLevelOfCoroutineForTrace($coroutine);
        if ($index !== null) {
            $level += $index;
            $limit = 1;
        } else {
            $limit = 0;
        }
        $trace = $coroutine->getTrace($level, $limit);
        if ($index !== null) {
            $trace = $trace[0] ?? [];
        }

        return $trace;
    }

    /**
     * @return array{
     *     'id': int,
     *     'state': string,
     *     'switches': int,
     *     'elapsed': string,
     *     'executing': string|null,
     *     'source_position': string|null,
     * } $simpleInfo
     * @psalm-return ($whatAreYouDoing is true ?array{
     *     'id': int,
     *     'state': string,
     *     'switches': int,
     *     'elapsed': string,
     *     'executing': string,
     *     'source_position': string,
     * } : array{
     *     'id': int,
     *     'state': string,
     *     'switches': int,
     *     'elapsed': string,
     * }) $simpleInfo
     */
    public static function getSimpleInfoOfCoroutine(Coroutine $coroutine, bool $showTraceInfo): array
    {
        $info = [
            'id' => $coroutine->getId(),
            'state' => static::getStateNameOfCoroutine($coroutine),
            'switches' => $coroutine->getSwitches(),
            'elapsed' => $coroutine->getElapsedAsString(),
        ];
        if ($showTraceInfo) {
            $frame = static::getTraceOfCoroutine($coroutine, 0);
            $info['executing'] = DebuggerHelper::convertFrameToExecutingString($frame);
            $file = $frame['file'] ?? null;
            $line = $frame['line'] ?? 0;
            if ($file === null) {
                $sourcePosition = '<internal space>';
            } else {
                $file = basename($file);
                $sourcePosition = "{$file}({$line})";
            }
            $info['source_position'] = $sourcePosition;
        }

        return $info;
    }

    /** We need to subtract the number of call stack layers of the Debugger itself */
    protected static function getCoroutineTraceDiffLevel(Coroutine $coroutine, string $name): int
    {
        static $diffLevelCache = [];
        if (isset($diffLevelCache[$name])) {
            return $diffLevelCache[$name];
        }

        $trace = $coroutine->getTrace();
        $diffLevel = 0;
        foreach ($trace as $index => $frame) {
            $class = $frame['class'] ?? '';
            if (is_a($class, self::class, true)) {
                $diffLevel = $index;
            }
        }
        /* Debugger::breakPointHandler() or something like it are not the Debugger frame,
         * but we do not need to -1 here because index is start with 0. */
        if ($coroutine === Coroutine::getCurrent()) {
            $diffLevel -= 1; /* we are in getTrace() here */
        }

        return $diffLevelCache[$name] = $diffLevel;
    }

    /**
     * @param array<array{
     *     'file': string|null,
     *     'line': int|null,
     * }> $trace
     * @return array<array{
     *     'line': string,
     *     'content': string,
     * }>
     */
    protected static function getSourceFileContentByTrace(array $trace, int $frameIndex, ?SplFileObject &$sourceFile = null, ?int &$sourceFileLine = null): array
    {
        /* init */
        $sourceFile = null;
        $sourceFileLine = 0;
        /* get frame info */
        $frame = $trace[$frameIndex] ?? null;
        if (!$frame || empty($frame['file']) || !isset($frame['line'])) {
            return [];
        }
        $file = $frame['file'];
        if (!file_exists($file)) {
            throw new DebuggerException('Source File not found');
        }
        $line = $frame['line'];
        if (!is_numeric($line)) {
            throw new DebuggerException('Invalid source file line no');
        }
        $line = (int) $line;
        // $class = $frame['class'] ?? '';
        // $function = $frame['function'] ?? '';
        // if (is_a($class, self::class, true) && $function === 'breakPointHandler') {
        //     $line -= 1;
        // }
        $sourceFileLine = $line;

        return DebuggerHelper::getSourceFileContentAsTable($file, $line, $sourceFile);
    }
}
