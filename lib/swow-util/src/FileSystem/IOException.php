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

namespace Swow\Util\FileSystem;

use ReflectionProperty;
use Swow\Exception;
use Throwable;
use function error_get_last;

class IOException extends Exception
{
    final public function __construct(string $message, int $code = 0, ?Throwable $previous = null, protected string $path = 'Unknown')
    {
        parent::__construct($message, $code, $previous);
    }

    public function getPath(): string
    {
        return $this->path;
    }

    public static function getLast(): static
    {
        $lastError = error_get_last();
        $exception = new static($lastError['message']);

        $rp = new ReflectionProperty(Exception::class, 'file');
        $rp->setAccessible(true);
        $rp->setValue($exception, $lastError['file']);

        $rp = new ReflectionProperty(Exception::class, 'line');
        $rp->setAccessible(true);
        $rp->setValue($exception, $lastError['line']);

        return $exception;
    }
}
