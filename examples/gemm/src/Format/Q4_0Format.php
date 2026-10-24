<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Format;

use Swow\Examples\Gemm\Format;
use Swow\Examples\Gemm\Quantization\Q4_0;
use ValueError;

use function intdiv;

final class Q4_0Format implements Format
{
    public function name(): string
    {
        return 'q4_0';
    }

    public function validateColumns(int $columns): void
    {
        if ($columns <= 0 || $columns % Q4_0::BLOCK_SIZE !== 0) {
            throw new ValueError('q4_0 columns must be a positive multiple of 32');
        }
    }

    public function rowBytes(int $columns): int
    {
        return intdiv($columns, Q4_0::BLOCK_SIZE) * Q4_0::BLOCK_BYTES;
    }

    public function generate(int $rows, int $columns, int $seed): string
    {
        return Q4_0::generate(intdiv($rows * $columns, Q4_0::BLOCK_SIZE), $seed);
    }

    /** @param list<float> $input @return list<float> */
    public function matvec(string $rows, array $input, int $rowCount, int $columns): array
    {
        return Q4_0::matvec($rows, $input, $rowCount, $columns);
    }

    /** @param list<float> $input */
    public function referenceRow(string $row, array $input, int $columns): float
    {
        $values = Q4_0::dequantize($row);
        $sum = 0.0;
        for ($index = 0; $index < $columns; $index++) {
            $sum += $values[$index] * $input[$index];
        }

        return $sum;
    }
}
