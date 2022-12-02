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

use Iterator;

final class ServerConnectionIterator implements ServerConnectionIteratorInterface
{
    /**
     * @param Iterator<ServerConnection, int> $iterator
     */
    public function __construct(protected Iterator $iterator)
    {
    }

    public function current(): ServerConnection
    {
        return $this->iterator->key();
    }

    public function next(): void
    {
        $this->iterator->next();
    }

    public function key(): int
    {
        return $this->iterator->current();
    }

    public function valid(): bool
    {
        return $this->iterator->valid();
    }

    public function rewind(): void
    {
        $this->iterator->rewind();
    }
}
