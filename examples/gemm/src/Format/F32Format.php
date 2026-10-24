<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Format;

use Swow\Examples\Gemm\Format;
use Swow\Examples\Gemm\Kernel\F32Gemv;
use Swow\Examples\Gemm\Support\Matrix;
use ValueError;

use function intdiv;
use function min;
use function pack;

final class F32Format implements Format
{
    /* Keep one generation chunk near 1M floats so a wide matrix does not need
     * one huge PHP array */
    private const CHUNK_FLOATS = 1 << 20;

    public function name(): string
    {
        return 'f32';
    }

    public function validateColumns(int $columns): void
    {
        if ($columns <= 0) {
            throw new ValueError('f32 columns must be positive');
        }
    }

    public function rowBytes(int $columns): int
    {
        return $columns * 4;
    }

    public function generate(int $rows, int $columns, int $seed): string
    {
        $data = '';
        $blockRows = max(1, intdiv(self::CHUNK_FLOATS, $columns));
        for ($begin = 0; $begin < $rows; $begin += $blockRows) {
            $end = min($rows, $begin + $blockRows);
            $data .= pack(
                'g*',
                ...Matrix::fillRange(($end - $begin) * $columns, $seed, $begin * $columns),
            );
        }

        return $data;
    }

    /** @param list<float> $input @return list<float> */
    public function matvec(string $rows, array $input, int $rowCount, int $columns): array
    {
        return F32Gemv::matvec($rows, $input, $rowCount, $columns);
    }

    /** @param list<float> $input */
    public function referenceRow(string $row, array $input, int $columns): float
    {
        $values = unpack('g*', $row);
        $sum = 0.0;
        for ($index = 0; $index < $columns; $index++) {
            $sum += $values[$index + 1] * $input[$index];
        }

        return $sum;
    }
}
