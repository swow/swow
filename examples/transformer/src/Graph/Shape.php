<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

use function array_product;

final class Shape
{
    /** @var list<int> */
    private array $dimensions;

    public function __construct(int ...$dimensions)
    {
        if ($dimensions === []) {
            throw new GraphException('Shape must have at least one dimension');
        }
        foreach ($dimensions as $dimension) {
            if ($dimension <= 0) {
                throw new GraphException('Shape dimensions must be positive');
            }
        }
        $this->dimensions = $dimensions;
    }

    /** @return list<int> */
    public function dimensions(): array
    {
        return $this->dimensions;
    }

    public function count(): int
    {
        return array_product($this->dimensions);
    }

    public function equals(self $other): bool
    {
        return $this->dimensions === $other->dimensions;
    }

    public function __toString(): string
    {
        return implode('x', $this->dimensions);
    }
}
