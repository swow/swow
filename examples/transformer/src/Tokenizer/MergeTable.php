<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tokenizer;

use function count;

final class MergeTable
{
    /** @var array<string, int> */
    private array $ranks = [];

    /** @param list<string> $merges */
    public function __construct(array $merges)
    {
        foreach ($merges as $rank => $merge) {
            $parts = explode(' ', trim($merge), 2);
            if (count($parts) !== 2) {
                continue;
            }
            $this->ranks[$parts[0] . "\0" . $parts[1]] = $rank;
        }
    }

    public function rank(string $left, string $right): ?int
    {
        return $this->ranks[$left . "\0" . $right] ?? null;
    }
}
