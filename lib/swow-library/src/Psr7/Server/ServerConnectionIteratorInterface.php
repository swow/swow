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

interface ServerConnectionIteratorInterface extends Iterator
{
    public function current(): ServerConnection;

    public function next(): void;

    public function key(): int;

    public function valid(): bool;

    public function rewind(): void;
}
