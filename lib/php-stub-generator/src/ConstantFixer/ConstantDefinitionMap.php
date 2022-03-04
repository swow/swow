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

use ArrayAccess;
use ArrayIterator;
use Exception;
use IteratorAggregate;
use Traversable;

class ConstantDefinitionMap implements ArrayAccess, IteratorAggregate
{
    /** @var array<string, ConstantDefinition> */
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

    /**
     * @param string $offset
     */
    public function offsetUnset(mixed $offset): void
    {
        unset($this->map[$offset]);
    }

    /**
     * @param string $offset
     */
    public function offsetExists(mixed $offset): bool
    {
        return isset($this->map[$offset]);
    }

    /**
     * @param string $offset
     * @return ConstantDefinition
     */
    public function offsetGet(mixed $offset): mixed
    {
        return $this->map[$offset];
    }

    /**
     * @param string $offset
     * @param ConstantDefinition $value
     */
    public function offsetSet(mixed $offset, mixed $value): void
    {
        if (!$value instanceof ConstantDefinition) {
            throw new Exception('bad invoke');
        }
        $this->map[$offset] = $value;
    }

    /**
     * @return Traversable<string, ConstantDefinition>
     */
    public function getIterator(): Traversable
    {
        return new ArrayIterator($this->map);
    }
}
