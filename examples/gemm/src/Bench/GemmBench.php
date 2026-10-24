<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Bench;

use Swow\Examples\Gemm\Kernel\Gemm;
use Swow\Examples\Gemm\Support\Matrix;
use Swow\Examples\Gemm\Support\System;
use Swow\Examples\Gemm\Support\Threads;
use Swow\SharedBuffer;
use Throwable;
use ValueError;

use function abs;
use function file_put_contents;
use function hrtime;
use function intdiv;
use function microtime;
use function printf;
use function sprintf;

use const FILE_APPEND;
use const PHP_VERSION;

/*
 * Square GEMM benchmark.
 *
 * Both matrices are published once in shared memory. A worker computes a row
 * range, measures its own time, and writes the rows back.
 */
final class GemmBench
{
    private const ERROR_TOLERANCE = 1e-3;

    /**
     * @return array{wall: float, seconds: list<float>, error: float}
     */
    private static function measure(int $size, int $threads): array
    {
        $left = Matrix::fill($size, Matrix::WEIGHT_SEED);
        $right = Matrix::fill($size, Matrix::INPUT_SEED);
        $leftBuffer = new SharedBuffer($size * $size * 4);
        $leftBuffer->writeF32(0, $left);
        $rightBuffer = new SharedBuffer($size * $size * 4);
        $rightBuffer->writeF32(0, $right);
        $resultBuffer = new SharedBuffer($size * $size * 4);
        $statsBuffer = new SharedBuffer($threads * 8);
        $errorsBuffer = new SharedBuffer($threads);

        $started = microtime(true);
        $workers = [];
        for ($id = 0; $id < $threads; $id++) {
            $workers[] = Threads::spawn(self::class . '::runWorker', [
                $leftBuffer,
                $rightBuffer,
                $resultBuffer,
                $statsBuffer,
                $errorsBuffer,
                $id,
                $threads,
                $size,
            ]);
        }
        Threads::join($workers);
        $wall = microtime(true) - $started;

        for ($id = 0; $id < $threads; $id++) {
            if ($errorsBuffer->viewU8($id, 1)[0] !== 0) {
                throw new ValueError("Worker {$id} failed");
            }
        }

        $result = $resultBuffer->viewF32(0, $size * $size);

        return [
            'wall' => $wall,
            'seconds' => $statsBuffer->viewF64(0, $threads),
            'error' => self::check($left, $right, $result, $size),
        ];
    }

    /** @param list<float> $left @param list<float> $right @param list<float> $result */
    private static function check(array $left, array $right, array $result, int $size): float
    {
        $worst = 0.0;
        foreach ([[0, 0], [0, 1], [1, 0], [1, 1], [2, 3], [3, 2]] as [$rowFraction, $columnFraction]) {
            $row = $rowFraction === 0 ? 0 : ($rowFraction === 1 ? $size - 1 : intdiv($size, $rowFraction + 1));
            $column = $columnFraction === 0
                ? 0
                : ($columnFraction === 1 ? $size - 1 : intdiv($size, $columnFraction + 1));
            $expected = 0.0;
            for ($k = 0; $k < $size; $k++) {
                $expected += $left[$row * $size + $k] * $right[$k * $size + $column];
            }
            $actual = $result[$row * $size + $column];
            $error = abs(($actual - $expected) / ($expected !== 0.0 ? $expected : 1.0));
            if ($error > $worst) {
                $worst = $error;
            }
        }

        return $worst;
    }

    public static function run(int $size, int $threads, bool $efficiency): void
    {
        if ($size <= 0 || $threads <= 0 || $threads > $size) {
            throw new ValueError('size must be positive and threads between 1 and size');
        }

        $parallel = self::measure($size, $threads);
        $operations = (float) $size * $size * $size;
        $rate = $operations / $parallel['wall'] / 1e9;

        printf("gemm: size=%d threads=%d\n", $size, $threads);
        printf(
            "gemm: parallel %dx%d in %.3f s, %.2f GMAC/s, %.2f GFLOP/s\n",
            $size,
            $size,
            $parallel['wall'],
            $rate,
            2.0 * $rate,
        );
        foreach ($parallel['seconds'] as $id => $seconds) {
            printf(
                "gemm: worker %2d rows [%5d, %5d) took %.3f s\n",
                $id,
                intdiv($size * $id, $threads),
                intdiv($size * ($id + 1), $threads),
                $seconds,
            );
        }
        printf(
            "gemm: php=%s jit=%s cpu=%s\n",
            PHP_VERSION,
            System::jit(),
            System::cpuName(),
        );
        self::reportCheck($parallel['error']);

        if ($efficiency) {
            $single = self::measure($size, 1);
            $singleRate = $operations / $single['wall'] / 1e9;
            printf(
                "gemm: single   %dx%d in %.3f s, %.2f GMAC/s\n",
                $size,
                $size,
                $single['wall'],
                $singleRate,
            );
            printf(
                "gemm: speedup %.2fx, efficiency %.1f%%\n",
                $rate / $singleRate,
                $rate / $singleRate / $threads * 100.0,
            );
            self::reportCheck($single['error']);
        }
    }

    private static function reportCheck(float $error): void
    {
        printf(
            "gemm: sampled check worst=%.3e (%s)\n",
            $error,
            $error < self::ERROR_TOLERANCE ? 'ok' : 'MISMATCH',
        );
        if ($error >= self::ERROR_TOLERANCE) {
            throw new ValueError('gemm result mismatch');
        }
    }

    public static function runWorker(
        SharedBuffer $left,
        SharedBuffer $right,
        SharedBuffer $result,
        SharedBuffer $stats,
        SharedBuffer $errors,
        int $id,
        int $threads,
        int $size,
    ): void {
        try {
            $begin = intdiv($size * $id, $threads);
            $end = intdiv($size * ($id + 1), $threads);
            $leftValues = $left->viewF32(0, $size * $size);
            $rightValues = $right->viewF32(0, $size * $size);
            $started = hrtime(true);
            $values = Gemm::rows($leftValues, $rightValues, $size, $begin, $end);
            $stats->writeF64($id * 8, [(hrtime(true) - $started) / 1e9]);
            $result->writeF32($begin * $size * 4, $values);
        } catch (Throwable $e) {
            $errors->writeU8($id, [1]);
            file_put_contents('php://stderr', sprintf("gemm worker %d: %s\n", $id, $e), FILE_APPEND);
        }
    }
}
