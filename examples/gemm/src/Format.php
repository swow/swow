<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm;

/*
 * A weight format: how to generate a matrix, how many bytes one row takes, and
 * how to run a matvec over a slice of rows.
 *
 * All methods work on packed bytes in native little-endian order. A worker
 * reads its own row range from the shared weight buffer and passes it here, so
 * a format never sees more than the rows it computes.
 */
interface Format
{
    public function name(): string;

    public function validateColumns(int $columns): void;

    public function rowBytes(int $columns): int;

    /** Generate a whole matrix as packed bytes */
    public function generate(int $rows, int $columns, int $seed): string;

    /**
     * @param list<float> $input
     * @return list<float>
     */
    public function matvec(string $rows, array $input, int $rowCount, int $columns): array;

    /**
     * Independent reference for one row, used by the sampled check.
     *
     * @param list<float> $input
     */
    public function referenceRow(string $row, array $input, int $columns): float;
}
