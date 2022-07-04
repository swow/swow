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

trait ExceptionFakerTrait
{
    protected function cloneInternalPropertiesFrom(Exception $exception): static
    {
        $rp = new ReflectionProperty(Exception::class, 'file');
        $rp->setAccessible(true);
        $rp->setValue($this, $exception->getFile());

        $rp = new ReflectionProperty(Exception::class, 'line');
        $rp->setAccessible(true);
        $rp->setValue($this, $exception->getLine());

        $rp = new ReflectionProperty(Exception::class, 'trace');
        $rp->setAccessible(true);
        $rp->setValue($this, $exception->getTrace());

        return $this;
    }
}
