<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

interface LogitProcessor
{
    /** @param list<float> $logits */
    public function process(array &$logits, SamplingContext $context, SamplingConfig $config): void;
}
