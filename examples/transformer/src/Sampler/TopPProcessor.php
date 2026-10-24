<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

use function count;
use function exp;
use function usort;

use const INF;

final class TopPProcessor implements LogitProcessor
{
    public function process(array &$logits, SamplingContext $context, SamplingConfig $config): void
    {
        $p = $config->topP;
        if ($p >= 1.0) {
            return;
        }
        $candidates = [];
        foreach ($logits as $id => $value) {
            if (is_finite($value)) {
                $candidates[] = [$id, $value];
            }
        }
        if (count($candidates) <= 1) {
            return;
        }
        usort($candidates, static fn(array $a, array $b): int => $b[1] <=> $a[1]);
        $maximum = $candidates[0][1];
        $sum = 0.0;
        foreach ($candidates as $candidate) {
            $sum += exp($candidate[1] - $maximum);
        }
        $cumulative = 0.0;
        $keep = [];
        foreach ($candidates as $candidate) {
            $keep[$candidate[0]] = true;
            $cumulative += exp($candidate[1] - $maximum);
            if ($cumulative / $sum >= $p) {
                break;
            }
        }
        foreach ($logits as $id => $value) {
            if (!isset($keep[$id])) {
                $logits[$id] = -INF;
            }
        }
    }
}
