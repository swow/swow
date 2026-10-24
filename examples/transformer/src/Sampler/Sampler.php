<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

use RuntimeException;

use function count;
use function exp;
use function is_finite;

use const INF;

final class Sampler
{
    private PenaltyProcessor $penalties;
    private TopKProcessor $topK;
    private TopPProcessor $topP;
    private MinPProcessor $minP;
    private TemperatureProcessor $temperature;

    public function __construct(
        private SamplingConfig $config = new SamplingConfig(),
        private ?RandomSource $random = null,
    ) {
        $this->penalties = new PenaltyProcessor();
        $this->topK = new TopKProcessor();
        $this->topP = new TopPProcessor();
        $this->minP = new MinPProcessor();
        $this->temperature = new TemperatureProcessor();
    }

    public function config(): SamplingConfig
    {
        return $this->config;
    }

    /** @param list<float> $logits */
    public function sample(array $logits, ?SamplingContext $context = null): int
    {
        $context ??= new SamplingContext();
        $logits = array_values($logits);
        if ($logits === []) {
            throw new RuntimeException('Cannot sample from empty logits');
        }
        $this->penalties->process($logits, $context, $this->config);

        if ($this->config->temperature <= 0.0 || $this->config->topK === 1) {
            return $this->argmax($logits);
        }

        $this->topK->process($logits, $context, $this->config);
        $this->topP->process($logits, $context, $this->config);
        $this->minP->process($logits, $context, $this->config);
        $this->temperature->process($logits, $context, $this->config);

        $candidates = [];
        $maximum = -INF;
        foreach ($logits as $id => $value) {
            if (is_finite($value)) {
                $candidates[] = [$id, $value];
                if ($value > $maximum) {
                    $maximum = $value;
                }
            }
        }
        if ($candidates === []) {
            throw new RuntimeException('All logits are filtered out');
        }

        $sum = 0.0;
        $probabilities = [];
        foreach ($candidates as [$id, $value]) {
            $probability = exp($value - $maximum);
            $probabilities[] = [$id, $probability];
            $sum += $probability;
        }
        $random = $context->random ?? $this->random ??= new XorShift32($this->config->seed);
        $target = $random->nextFloat() * $sum;
        $cumulative = 0.0;
        foreach ($probabilities as [$id, $probability]) {
            $cumulative += $probability;
            if ($cumulative >= $target) {
                return $id;
            }
        }

        return $probabilities[count($probabilities) - 1][0];
    }

    /** @param list<float> $logits */
    private function argmax(array $logits): int
    {
        $best = 0;
        $maximum = -INF;
        foreach ($logits as $id => $value) {
            if ($value > $maximum) {
                $maximum = $value;
                $best = $id;
            }
        }
        if (!is_finite($maximum)) {
            throw new RuntimeException('All logits are -INF');
        }

        return $best;
    }
}
