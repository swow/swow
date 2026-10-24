<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

final class XorShift32 implements RandomSource
{
    private int $state;

    public function __construct(int $seed = 1)
    {
        $this->state = ($seed & 0xFFFFFFFF) ?: 0x6D2B79F5;
    }

    public function nextUInt32(): int
    {
        $x = $this->state;
        $x ^= ($x << 13) & 0xFFFFFFFF;
        $x ^= $x >> 17;
        $x ^= ($x << 5) & 0xFFFFFFFF;
        $this->state = $x & 0xFFFFFFFF;

        return $this->state;
    }

    public function nextFloat(): float
    {
        return ($this->nextUInt32() >> 8) / 16777216.0;
    }
}
