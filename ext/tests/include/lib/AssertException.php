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

class AssertException extends Exception
{
    public function __construct($message = '', $code = 0, ?Throwable $previous = null)
    {
        parent::__construct($message, $code, $previous);

        $rp = new ReflectionProperty(Exception::class, 'trace');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $trace = $rp->getValue($this);
        $file = null;
        foreach ($trace as $index => $frame) {
            if ($file !== null && $file !== $frame['file']) {
                $trace = array_slice($trace, $index);
                break;
            }
            $file ??= $frame['file'];
        }
        $rp->setValue($this, $trace);

        $rp = new ReflectionProperty(Exception::class, 'file');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $rp->setValue($this, $trace[0]['file']);

        $rp = new ReflectionProperty(Exception::class, 'line');
        if (PHP_VERSION_ID < 80100) {
            $rp->setAccessible(true);
        }
        $rp->setValue($this, $trace[0]['line']);
    }
}
