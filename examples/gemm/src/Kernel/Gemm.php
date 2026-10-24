<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Kernel;

use function array_fill;

/*
 * Square f32 GEMM, ikj order.
 *
 * Right rows stay contiguous, so the inner loop walks one row of the right
 * matrix and one output row without strided access.
 */
final class Gemm
{
    /**
     * @param list<float> $left
     * @param list<float> $right
     * @return list<float>
     */
    public static function rows(array $left, array $right, int $size, int $rowBegin, int $rowEnd): array
    {
        $result = [];
        for ($row = $rowBegin; $row < $rowEnd; $row++) {
            $output = array_fill(0, $size, 0.0);
            $leftBase = $row * $size;
            for ($k = 0; $k < $size; $k++) {
                $factor = $left[$leftBase + $k];
                $rightBase = $k * $size;
                for ($column = 0; $column < $size; $column++) {
                    $output[$column] += $factor * $right[$rightBase + $column];
                }
            }
            foreach ($output as $value) {
                $result[] = $value;
            }
        }

        return $result;
    }
}
