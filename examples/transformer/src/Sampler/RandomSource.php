<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

interface RandomSource
{
    public function nextUInt32(): int;

    public function nextFloat(): float;
}
