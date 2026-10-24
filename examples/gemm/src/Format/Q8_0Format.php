<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Format;

use Swow\Examples\Gemm\Format;
use Swow\Examples\Gemm\Quantization\Q8_0;
use ValueError;

use function intdiv;

final class Q8_0Format implements Format
{
    public function name(): string
    {
        return 'q8_0';
    }

    public function validateColumns(int $columns): void
    {
        if ($columns <= 0 || $columns % Q8_0::BLOCK_SIZE !== 0) {
            throw new ValueError('q8_0 columns must be a positive multiple of 32');
        }
    }

    public function rowBytes(int $columns): int
    {
        return intdiv($columns, Q8_0::BLOCK_SIZE) * Q8_0::BLOCK_BYTES;
    }

    public function generate(int $rows, int $columns, int $seed): string
    {
        return Q8_0::generate(intdiv($rows * $columns, Q8_0::BLOCK_SIZE), $seed);
    }

    /** @param list<float> $input @return list<float> */
    public function matvec(string $rows, array $input, int $rowCount, int $columns): array
    {
        return Q8_0::matvec($rows, $input, $rowCount, $columns);
    }

    /** @param list<float> $input */
    public function referenceRow(string $row, array $input, int $columns): float
    {
        /* The kernel quantizes the input too, so the reference must do the same.
         * dotQ8_0 is the slow path and does not share the fast kernel's code. */
        return Q8_0::dotQ8_0($row, Q8_0::quantize($input));
    }
}
