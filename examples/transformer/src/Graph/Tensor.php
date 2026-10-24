<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

use function array_values;
use function count;

final class Tensor
{
    /** @var list<float>|list<int> */
    private array $data;

    /**
     * @param list<float>|list<int> $data
     */
    public function __construct(
        private int $id,
        private string $name,
        private Shape $shape,
        private string $dtype,
        array $data,
    ) {
        DType::assertSupported($dtype);
        $this->data = array_values($data);
        if (count($this->data) !== $shape->count()) {
            throw new GraphException(
                "Data count does not match shape {$shape}: " . count($this->data),
            );
        }
    }

    public function id(): int
    {
        return $this->id;
    }

    public function name(): string
    {
        return $this->name;
    }

    public function shape(): Shape
    {
        return $this->shape;
    }

    public function dtype(): string
    {
        return $this->dtype;
    }

    /** @return list<float>|list<int> */
    public function data(): array
    {
        return $this->data;
    }
}
