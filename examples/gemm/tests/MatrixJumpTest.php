<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Tests;

use PHPUnit\Framework\TestCase;
use Swow\Examples\Gemm\Support\Matrix;

/**
 * @internal
 */
final class MatrixJumpTest extends TestCase
{
    public function testSkipMatchesSequentialGeneration(): void
    {
        foreach ([0, 1, 2, 7, 31, 32, 33, 1000, 123456] as $skip) {
            $this->assertSame(
                self::sequential(8, Matrix::WEIGHT_SEED, $skip),
                Matrix::fillRange(8, Matrix::WEIGHT_SEED, $skip),
                "skip={$skip}",
            );
        }
    }

    /** @return list<float> */
    private static function sequential(int $count, int $seed, int $skip): array
    {
        $state = $seed;
        for ($index = 0; $index < $skip; $index++) {
            self::advance($state);
        }
        $values = [];
        for ($index = 0; $index < $count; $index++) {
            self::advance($state);
            $values[] = (($state >> 8) / 16777216.0) * 2.0 - 1.0;
        }

        return $values;
    }

    private static function advance(int &$state): void
    {
        $state ^= ($state << 13) & 0xFFFFFFFF;
        $state ^= $state >> 17;
        $state ^= ($state << 5) & 0xFFFFFFFF;
        $state &= 0xFFFFFFFF;
    }
}
