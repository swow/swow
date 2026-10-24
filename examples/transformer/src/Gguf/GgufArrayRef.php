<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Gguf;

final class GgufArrayRef
{
    public function __construct(
        private GgufFile $file,
        private int $offset,
        private int $elementType,
        private int $count,
    ) {
    }

    public function offset(): int
    {
        return $this->offset;
    }

    public function elementType(): int
    {
        return $this->elementType;
    }

    public function count(): int
    {
        return $this->count;
    }

    /** @return list<mixed> */
    public function toArray(): array
    {
        return $this->file->readArray($this);
    }
}
