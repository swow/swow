<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Thread;

use RuntimeException;
use Swow\Examples\Gemm\Kernel\F32Gemv;
use Swow\Examples\Gemm\Quantization\Q4_0;
use Swow\Examples\Gemm\Quantization\Q8_0;
use Swow\Examples\Transformer\Graph\Weight;

use function array_map;
use function intdiv;

/*
 * Linear kernels.
 *
 * Inside a cooperative worker, a linear op splits its rows, writes its own
 * range into the shared output buffer, waits at the op barrier, then reads the
 * full result. Without a context (single-thread use, tests), it computes the
 * whole result locally.
 */
final class Linear
{
    private const MIN_PARALLEL_WORK = 1_000_000;

    private const BARRIER_TIMEOUT = 60_000;

    /**
     * @param list<float> $input
     * @return list<float>
     */
    public static function matvec(Weight $weight, array $input): array
    {
        return self::matvecMany([$weight], $input)[0];
    }

    /**
     * @param list<Weight> $weights
     * @param list<float> $input
     * @return list<list<float>>
     */
    public static function matvecMany(array $weights, array $input): array
    {
        $context = CooperativeContext::current();
        if ($context === null || !self::canSplit($weights) || self::work($weights) < self::MIN_PARALLEL_WORK) {
            return array_map(
                static fn(Weight $weight): array => self::compute(
                    $weight->data(),
                    $weight->type(),
                    $input,
                    $weight->rows(),
                    $weight->columns(),
                ),
                $weights,
            );
        }

        $buffer = $context->outputBuffer($context->opIndex());
        $offset = 0;
        $ranges = [];
        foreach ($weights as $weight) {
            $rows = $weight->rows();
            $columns = $weight->columns();
            $begin = intdiv($rows * $context->rank, $context->threads);
            $end = intdiv($rows * ($context->rank + 1), $context->threads);
            $values = self::compute(
                $weight->slice($begin, $end),
                $weight->type(),
                $input,
                $end - $begin,
                $columns,
            );
            $buffer->writeF64($offset + $begin * 8, $values);
            $ranges[] = [$offset, $rows];
            $offset += $rows * 8;
        }

        if (!$context->opBarrier->wait(self::BARRIER_TIMEOUT)) {
            throw new RuntimeException('Linear op barrier timed out');
        }

        $result = [];
        foreach ($ranges as [$base, $rows]) {
            $result[] = $buffer->viewF64($base, $rows);
        }
        $context->advanceOp();

        return $result;
    }

    /** @param list<Weight> $weights */
    private static function canSplit(array $weights): bool
    {
        foreach ($weights as $weight) {
            if ($weight->tensorName() === null || $weight->sourcePath() === null) {
                return false;
            }
        }

        return true;
    }

    /** @param list<Weight> $weights */
    private static function work(array $weights): int
    {
        $work = 0;
        foreach ($weights as $weight) {
            $work += $weight->rows() * $weight->columns();
        }

        return $work;
    }

    /**
     * @param list<float> $input
     * @return list<float>
     */
    private static function compute(string $data, string $type, array $input, int $rows, int $columns): array
    {
        return match ($type) {
            Weight::Q8_0 => Q8_0::matvec($data, $input, $rows, $columns),
            Weight::Q4_0 => Q4_0::matvec($data, $input, $rows, $columns),
            Weight::F32 => F32Gemv::matvec($data, $input, $rows, $columns),
            default => throw new RuntimeException('Unsupported weight type: ' . $type),
        };
    }
}
