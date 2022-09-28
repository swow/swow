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

namespace Swow\Utils\FileSystem;

use ReflectionProperty;
use Swow\Exception;
use Throwable;

use function error_get_last;

final class IOException extends Exception
{
    protected string $path = 'Unknown';

    public function __construct(string $message, int $code = 0, ?Throwable $previous = null)
    {
        parent::__construct($message, $code, $previous);
    }

    public function getPath(): string
    {
        return $this->path;
    }

    public static function createFromLastError(): self
    {
        $lastError = error_get_last();
        $exception = new self($lastError['message']);

        $rp = new ReflectionProperty(Exception::class, 'file');
        $rp->setAccessible(true);
        $rp->setValue($exception, $lastError['file']);

        $rp = new ReflectionProperty(Exception::class, 'line');
        $rp->setAccessible(true);
        $rp->setValue($exception, $lastError['line']);

        return $exception;
    }
}
