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

namespace Swow\Psr7\Server;

use Exception;
use WeakMap;

class BroadcastResult
{
    /**
     * @param ?WeakMap<ServerConnection, Exception> $exceptions
     */
    public function __construct(protected int $count, protected int $failureCount, protected ?WeakMap $exceptions = null)
    {
    }

    public function getCount(): int
    {
        return $this->count;
    }

    public function getSuccessCount(): int
    {
        return $this->count - $this->failureCount;
    }

    public function getFailureCount(): int
    {
        return $this->failureCount;
    }

    /** @return ?WeakMap<ServerConnection, Exception> $exceptions */
    public function getExceptions(): ?WeakMap
    {
        return $this->exceptions;
    }
}
