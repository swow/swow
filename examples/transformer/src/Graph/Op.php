<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

final class Op
{
    /**
     * @param list<int> $inputIds
     * @param list<Shape> $inputShapes
     * @param list<string> $inputDtypes
     * @param array<string, mixed> $attrs
     */
    public function __construct(
        private int $index,
        private string $name,
        private array $inputIds,
        private int $outputId,
        private array $inputShapes,
        private array $inputDtypes,
        private Shape $outputShape,
        private string $outputDtype,
        private array $attrs = [],
    ) {
    }

    public function index(): int
    {
        return $this->index;
    }

    public function name(): string
    {
        return $this->name;
    }

    /** @return list<int> */
    public function inputIds(): array
    {
        return $this->inputIds;
    }

    public function outputId(): int
    {
        return $this->outputId;
    }

    /** @return list<Shape> */
    public function inputShapes(): array
    {
        return $this->inputShapes;
    }

    /** @return list<string> */
    public function inputDtypes(): array
    {
        return $this->inputDtypes;
    }

    public function outputShape(): Shape
    {
        return $this->outputShape;
    }

    public function outputDtype(): string
    {
        return $this->outputDtype;
    }

    /** @return array<string, mixed> */
    public function attrs(): array
    {
        return $this->attrs;
    }
}
