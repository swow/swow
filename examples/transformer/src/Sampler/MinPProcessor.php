<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

use function exp;
use function max;

use const INF;

final class MinPProcessor implements LogitProcessor
{
    public function process(array &$logits, SamplingContext $context, SamplingConfig $config): void
    {
        if ($config->minP <= 0.0) {
            return;
        }
        $maximum = -INF;
        foreach ($logits as $value) {
            if (is_finite($value)) {
                $maximum = max($maximum, $value);
            }
        }
        if (!is_finite($maximum)) {
            return;
        }
        $sum = 0.0;
        foreach ($logits as $value) {
            if (is_finite($value)) {
                $sum += exp($value - $maximum);
            }
        }
        if ($sum <= 0.0) {
            return;
        }
        $threshold = ($config->minP * 1.0 / $sum);
        foreach ($logits as $id => $value) {
            if (is_finite($value) && exp($value - $maximum) / $sum < $threshold) {
                $logits[$id] = -INF;
            }
        }
    }
}
