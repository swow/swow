<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Gguf;

interface WeightStore
{
    public function tensor(string $name): TensorInfo;

    public function read(string $name, int $offset, int $length): string;

    public function row(string $name, int $row): string;

    public function rows(string $name, int $rowBegin, int $rowEnd): string;
}
