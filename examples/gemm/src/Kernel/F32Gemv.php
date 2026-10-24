<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Kernel;

use function substr;
use function unpack;

/*
 * Dense f32 matvec over packed little-endian rows.
 *
 * The row is unpacked once, then the dot product runs eight lanes so the
 * dependency chain does not limit the loop.
 */
final class F32Gemv
{
    /**
     * @param list<float> $input
     * @return list<float>
     */
    public static function matvec(string $weights, array $input, int $rows, int $columns): array
    {
        $result = [];
        $rowBytes = $columns * 4;
        $limit = $columns - 7;
        for ($row = 0; $row < $rows; $row++) {
            $values = unpack('g*', substr($weights, $row * $rowBytes, $rowBytes));
            $sum0 = 0.0;
            $sum1 = 0.0;
            $sum2 = 0.0;
            $sum3 = 0.0;
            $sum4 = 0.0;
            $sum5 = 0.0;
            $sum6 = 0.0;
            $sum7 = 0.0;
            $index = 0;
            for (; $index < $limit; $index += 8) {
                $sum0 += $values[$index + 1] * $input[$index];
                $sum1 += $values[$index + 2] * $input[$index + 1];
                $sum2 += $values[$index + 3] * $input[$index + 2];
                $sum3 += $values[$index + 4] * $input[$index + 3];
                $sum4 += $values[$index + 5] * $input[$index + 4];
                $sum5 += $values[$index + 6] * $input[$index + 5];
                $sum6 += $values[$index + 7] * $input[$index + 6];
                $sum7 += $values[$index + 8] * $input[$index + 7];
            }
            for (; $index < $columns; $index++) {
                $sum0 += $values[$index + 1] * $input[$index];
            }
            $result[] = $sum0 + $sum1 + $sum2 + $sum3 + $sum4 + $sum5 + $sum6 + $sum7;
        }

        return $result;
    }
}
