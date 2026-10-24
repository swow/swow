<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Sampler;

use function count;

use const INF;

final class TopKProcessor implements LogitProcessor
{
    public function process(array &$logits, SamplingContext $context, SamplingConfig $config): void
    {
        $size = count($logits);
        $k = $config->topK;
        if ($k <= 0 || $k >= $size) {
            return;
        }

        /** @var list<float> $heap */
        $heap = [];
        /** @var list<int> $ids */
        $ids = [];
        foreach ($logits as $id => $value) {
            if (!is_finite($value)) {
                continue;
            }
            if (count($heap) < $k) {
                $heap[] = $value;
                $ids[] = $id;
                $this->siftUp($heap, $ids, count($heap) - 1);
            } elseif ($value > $heap[0]) {
                $heap[0] = $value;
                $ids[0] = $id;
                $this->siftDown($heap, $ids, 0);
            }
        }

        $keep = [];
        foreach ($ids as $id) {
            $keep[$id] = true;
        }
        foreach ($logits as $id => $value) {
            if (!isset($keep[$id])) {
                $logits[$id] = -INF;
            }
        }
    }

    /** @param list<float> $heap @param list<int> $ids */
    private function siftUp(array &$heap, array &$ids, int $index): void
    {
        while ($index > 0) {
            $parent = intdiv($index - 1, 2);
            if ($heap[$parent] <= $heap[$index]) {
                return;
            }
            [$heap[$parent], $heap[$index]] = [$heap[$index], $heap[$parent]];
            [$ids[$parent], $ids[$index]] = [$ids[$index], $ids[$parent]];
            $index = $parent;
        }
    }

    /** @param list<float> $heap @param list<int> $ids */
    private function siftDown(array &$heap, array &$ids, int $index): void
    {
        $size = count($heap);
        while (true) {
            $left = $index * 2 + 1;
            $right = $left + 1;
            $smallest = $index;
            if ($left < $size && $heap[$left] < $heap[$smallest]) {
                $smallest = $left;
            }
            if ($right < $size && $heap[$right] < $heap[$smallest]) {
                $smallest = $right;
            }
            if ($smallest === $index) {
                return;
            }
            [$heap[$smallest], $heap[$index]] = [$heap[$index], $heap[$smallest]];
            [$ids[$smallest], $ids[$index]] = [$ids[$index], $ids[$smallest]];
            $index = $smallest;
        }
    }
}
