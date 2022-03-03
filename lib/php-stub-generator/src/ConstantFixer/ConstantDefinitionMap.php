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

namespace Swow\StubUtils\ConstantFixer;

use \Exception;
use \ArrayAccess;
use \IteratorAggregate;
use \Traversable;
use \ArrayIterator;

class ConstantDefinitionMap implements ArrayAccess, IteratorAggregate
{
    private array $map = [];
    public function __construct(
        private string $arch,
        private string $os,
    ) {
    }
    public function getOS(): string
    {
        return $this->os;
    }
    public function getArch(): string
    {
        return $this->arch;
    }
    public function offsetUnset(mixed $offset): void
    {
        unset($this->map[$offset]);
    }
    public function offsetExists(mixed $offset): bool
    {
        return isset($this->map[$offset]);
    }
    public function offsetGet(mixed $offset): mixed
    {
        return $this->map[$offset];
    }
    public function offsetSet(mixed $offset, mixed $value): void
    {
        if (!$value instanceof ConstantDefinition) {
            throw new Exception('bad invoke');
        }
        $this->map[$offset] = $value;
    }
    public function getIterator(): Traversable
    {
        return new ArrayIterator($this->map);
    }
}
