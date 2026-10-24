<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

final class SamplingContext
{
    /** @param list<int> $history */
    public function __construct(
        public array $history = [],
        public int $position = 0,
        public ?RandomSource $random = null,
    ) {
    }
}
