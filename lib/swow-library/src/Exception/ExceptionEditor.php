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

namespace Swow\Exception;

use Exception;
use ReflectionProperty;

use const PHP_VERSION_ID;

class ExceptionEditor
{
    public static function setCode(Exception $exception, int $code): void
    {
        $rp = new ReflectionProperty(Exception::class, 'code');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $rp->setValue($exception, $code);
    }

    public static function setMessage(Exception $exception, string $message): void
    {
        $rp = new ReflectionProperty(Exception::class, 'message');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $rp->setValue($exception, $message);
    }

    public static function setFile(Exception $exception, string $file): void
    {
        $rp = new ReflectionProperty(Exception::class, 'file');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $rp->setValue($exception, $file);
    }

    public static function setLine(Exception $exception, int $line): void
    {
        $rp = new ReflectionProperty(Exception::class, 'line');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $rp->setValue($exception, $line);
    }

    /**
     * @param array<array<string, mixed>> $trace
     * @phpstan-param array<array{
     *     'file': string,
     *     'line': int,
     *     'function': string,
     *     'args': array<mixed>,
     *     "class"?: string
     * }> $trace
     */
    public static function setTrace(Exception $exception, array $trace): void
    {
        $rp = new ReflectionProperty(Exception::class, 'trace');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $rp->setValue($exception, $trace);
    }
}
