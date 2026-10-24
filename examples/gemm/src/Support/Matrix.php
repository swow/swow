<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Support;

final class Matrix
{
    public const WEIGHT_SEED = 2024;

    public const INPUT_SEED = 7;

    /** @var list<list<int>>|null */
    private static ?array $jumpPowers = null;

    /** @return list<float> */
    public static function fill(int $size, int $seed): array
    {
        return self::fillRange($size * $size, $seed);
    }

    /** @return list<float> */
    public static function fillRange(int $count, int $seed, int $skip = 0): array
    {
        $matrix = [];
        $state = $skip > 0 ? self::jumpState($seed, $skip) : $seed;
        for ($index = 0; $index < $count; $index++) {
            $matrix[] = self::next($state);
        }

        return $matrix;
    }

    private static function next(int &$state): float
    {
        $state = self::step($state);

        return (($state >> 8) / 16777216.0) * 2.0 - 1.0;
    }

    private static function step(int $state): int
    {
        $state ^= ($state << 13) & 0xFFFFFFFF;
        $state ^= $state >> 17;
        $state ^= ($state << 5) & 0xFFFFFFFF;

        return $state & 0xFFFFFFFF;
    }

    /** Advance the xorshift state by $steps in O(log $steps) */
    private static function jumpState(int $state, int $steps): int
    {
        $powers = self::jumpPowers();
        $level = 0;
        while ($steps > 0) {
            if (($steps & 1) !== 0) {
                $state = self::applyMatrix($powers[$level], $state);
            }
            $steps >>= 1;
            $level++;
        }

        return $state;
    }

    /** @return list<list<int>> */
    private static function jumpPowers(): array
    {
        if (self::$jumpPowers !== null) {
            return self::$jumpPowers;
        }

        $first = [];
        for ($bit = 0; $bit < 32; $bit++) {
            $first[$bit] = self::step(1 << $bit);
        }
        $powers = [$first];
        for ($level = 1; $level < 32; $level++) {
            $powers[$level] = self::compose($powers[$level - 1], $powers[$level - 1]);
        }

        return self::$jumpPowers = $powers;
    }

    /** @param list<int> $matrix */
    private static function applyMatrix(array $matrix, int $state): int
    {
        $result = 0;
        for ($bit = 0; $bit < 32; $bit++) {
            if (($state & (1 << $bit)) !== 0) {
                $result ^= $matrix[$bit];
            }
        }

        return $result;
    }

    /** @param list<int> $left @param list<int> $right @return list<int> */
    private static function compose(array $left, array $right): array
    {
        $result = [];
        for ($bit = 0; $bit < 32; $bit++) {
            $result[$bit] = self::applyMatrix($left, $right[$bit]);
        }

        return $result;
    }
}
