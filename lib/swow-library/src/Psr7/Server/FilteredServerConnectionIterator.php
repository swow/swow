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

use Closure;
use Iterator;

final class FilteredServerConnectionIterator implements ServerConnectionIteratorInterface
{
    /**
     * @param Iterator<ServerConnection, int> $iterator
     * @param Closure(ServerConnection): bool $filter
     */
    public function __construct(protected Iterator $iterator, protected Closure $filter) {}

    public function current(): ServerConnection
    {
        return $this->iterator->key();
    }

    public function next(): void
    {
        $this->iterator->next();
        $this->filter();
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
        $this->filter();
    }

    private function filter(): void
    {
        $iterator = $this->iterator;
        $filter = $this->filter;
        while ($iterator->valid() && !$filter($iterator->key())) {
            $iterator->next();
        }
    }
}
