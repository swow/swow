<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

use function array_slice;
use function count;

final class PenaltyProcessor implements LogitProcessor
{
    public function process(array &$logits, SamplingContext $context, SamplingConfig $config): void
    {
        if ($config->penaltyLastN <= 0) {
            return;
        }
        $recent = array_slice($context->history, -$config->penaltyLastN);
        if ($recent === []) {
            return;
        }
        $counts = [];
        foreach ($recent as $token) {
            if ($token >= 0 && $token < count($logits)) {
                $counts[$token] = ($counts[$token] ?? 0) + 1;
            }
        }
        foreach ($counts as $token => $count) {
            $value = $logits[$token];
            if ($config->repeatPenalty !== 1.0 && $config->repeatPenalty > 0.0) {
                $value = $value > 0.0
                    ? $value / $config->repeatPenalty
                    : $value * $config->repeatPenalty;
            }
            $value -= $count * $config->frequencyPenalty;
            $value -= $config->presencePenalty;
            $logits[$token] = $value;
        }
    }
}
