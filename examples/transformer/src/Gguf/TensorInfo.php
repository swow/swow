<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Gguf;

use function count;

final class TensorInfo
{
    /** @param list<int> $dimensions */
    public function __construct(
        private string $name,
        private array $dimensions,
        private int $ggmlType,
        private int $relativeOffset,
        private int $dataOffset,
        private int $byteSize,
    ) {
    }

    public function name(): string
    {
        return $this->name;
    }

    /** @return list<int> */
    public function dimensions(): array
    {
        return $this->dimensions;
    }

    public function ggmlType(): int
    {
        return $this->ggmlType;
    }

    public function typeName(): string
    {
        return GgmlType::name($this->ggmlType);
    }

    public function relativeOffset(): int
    {
        return $this->relativeOffset;
    }

    public function dataOffset(): int
    {
        return $this->dataOffset;
    }

    public function byteSize(): int
    {
        return $this->byteSize;
    }

    public function elementCount(): int
    {
        $count = 1;
        foreach ($this->dimensions as $dimension) {
            $count *= $dimension;
        }

        return $count;
    }

    public function columns(): int
    {
        return $this->dimensions[0] ?? 0;
    }

    public function rows(): int
    {
        $rows = 1;
        for ($index = 1, $count = count($this->dimensions); $index < $count; $index++) {
            $rows *= $this->dimensions[$index];
        }

        return $rows;
    }

    public function rowBytes(): int
    {
        return GgmlType::byteSize($this->ggmlType, $this->columns());
    }

    public function isQuantized(): bool
    {
        return GgmlType::blockSize($this->ggmlType) > 1;
    }

    public function bitsPerWeight(): float
    {
        return GgmlType::bytesPerBlock($this->ggmlType) * 8.0 / GgmlType::blockSize($this->ggmlType);
    }
}
