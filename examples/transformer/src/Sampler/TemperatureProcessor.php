<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

final class TemperatureProcessor implements LogitProcessor
{
    public function process(array &$logits, SamplingContext $context, SamplingConfig $config): void
    {
        if ($config->temperature <= 0.0 || $config->temperature === 1.0) {
            return;
        }
        foreach ($logits as $id => $value) {
            if (is_finite($value)) {
                $logits[$id] = $value / $config->temperature;
            }
        }
    }
}
