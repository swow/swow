<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Bench;

use Swow\Examples\Gemm\Format;
use Swow\Examples\Gemm\Support\CpuAffinity;
use Swow\SharedBuffer;
use Throwable;

use function count;
use function file_put_contents;
use function hrtime;
use function intdiv;
use function sort;

use const FILE_APPEND;

/*
 * One worker of the GEMV benchmark.
 *
 * The worker reads only its row range from the shared weight buffer, warms up,
 * measures each iteration, and writes the median and its result rows back.
 */
final class Worker
{
    public static function run(
        Format $format,
        SharedBuffer $weights,
        SharedBuffer $input,
        SharedBuffer $output,
        SharedBuffer $stats,
        SharedBuffer $errors,
        int $id,
        int $threads,
        int $rows,
        int $columns,
        ?int $cpu,
        int $warmup,
        int $iterations,
    ): void {
        try {
            if ($cpu !== null) {
                CpuAffinity::pin($cpu);
            }
            $begin = intdiv($rows * $id, $threads);
            $end = intdiv($rows * ($id + 1), $threads);
            $rowBytes = $format->rowBytes($columns);
            $slice = $weights->read($begin * $rowBytes, ($end - $begin) * $rowBytes);
            $vector = $input->viewF32(0, $columns);

            for ($iteration = 0; $iteration < $warmup; $iteration++) {
                $format->matvec($slice, $vector, $end - $begin, $columns);
            }

            $times = [];
            $values = [];
            for ($iteration = 0; $iteration < $iterations; $iteration++) {
                $started = hrtime(true);
                $values = $format->matvec($slice, $vector, $end - $begin, $columns);
                $times[] = (hrtime(true) - $started) / 1e9;
            }
            sort($times);
            $middle = intdiv(count($times), 2);
            $median = count($times) % 2 === 0
                ? ($times[$middle - 1] + $times[$middle]) / 2.0
                : $times[$middle];

            $stats->writeF64($id * 8, [$median]);
            $output->writeF32($begin * 4, $values);
        } catch (Throwable $e) {
            $errors->writeU8($id, [1]);
            file_put_contents('php://stderr', "gemv worker {$id}: {$e}\n", FILE_APPEND);
        }
    }
}
