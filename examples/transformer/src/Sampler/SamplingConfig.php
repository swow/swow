<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

final class SamplingConfig
{
    public function __construct(
        public int $topK = 40,
        public float $topP = 0.95,
        public float $minP = 0.05,
        public float $temperature = 0.8,
        public int $penaltyLastN = 64,
        public float $repeatPenalty = 1.0,
        public float $frequencyPenalty = 0.0,
        public float $presencePenalty = 0.0,
        public int $seed = 1,
    ) {
    }
}
