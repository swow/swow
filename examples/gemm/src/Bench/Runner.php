<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Bench;

use Swow\Examples\Gemm\Format;
use Swow\Examples\Gemm\Support\CpuAffinity;
use Swow\Examples\Gemm\Support\Matrix;
use Swow\Examples\Gemm\Support\System;
use Swow\Examples\Gemm\Support\Threads;
use Swow\SharedBuffer;
use ValueError;

use function abs;
use function array_unique;
use function array_values;
use function count;
use function fwrite;
use function implode;
use function intdiv;
use function max;
use function printf;
use function strlen;

use const PHP_VERSION;
use const STDERR;

/*
 * GEMV benchmark driver.
 *
 * One matrix, one input vector, one output vector. The parent generates the
 * weights once, publishes them in shared memory, starts the workers, and reads
 * their medians and results back.
 */
final class Runner
{
    private const ERROR_TOLERANCE = 1e-3;

    /**
     * @param list<int> $cpus
     * @return array{median: float, error: float, workers: list<float>}
     */
    private static function measure(
        Format $format,
        string $weights,
        array $input,
        int $k,
        int $n,
        int $threads,
        array $cpus,
        int $warmup,
        int $iterations,
    ): array {
        $weightBuffer = new SharedBuffer(strlen($weights));
        $weightBuffer->write(0, $weights);
        $inputBuffer = new SharedBuffer($k * 4);
        $inputBuffer->writeF32(0, $input);
        $outputBuffer = new SharedBuffer($n * 4);
        $statsBuffer = new SharedBuffer($threads * 8);
        $errorsBuffer = new SharedBuffer($threads);
        $cpuCount = count($cpus);

        $workers = [];
        for ($id = 0; $id < $threads; $id++) {
            $workers[] = Threads::spawn(Worker::class . '::run', [
                $format,
                $weightBuffer,
                $inputBuffer,
                $outputBuffer,
                $statsBuffer,
                $errorsBuffer,
                $id,
                $threads,
                $n,
                $k,
                $cpuCount > 0 ? $cpus[$id % $cpuCount] : null,
                $warmup,
                $iterations,
            ]);
        }
        Threads::join($workers);

        for ($id = 0; $id < $threads; $id++) {
            if ($errorsBuffer->viewU8($id, 1)[0] !== 0) {
                throw new ValueError("Worker {$id} failed");
            }
        }

        $result = $outputBuffer->viewF32(0, $n);
        $medians = $statsBuffer->viewF64(0, $threads);

        return [
            'median' => max($medians),
            'error' => self::check($format, $weights, $input, $result, $k, $n),
            'workers' => $medians,
        ];
    }

    /** @param list<float> $input @param list<float> $result */
    private static function check(
        Format $format,
        string $weights,
        array $input,
        array $result,
        int $k,
        int $n,
    ): float {
        $rowBytes = $format->rowBytes($k);
        $rows = array_values(array_unique([
            0,
            1,
            $n - 1,
            intdiv($n, 2),
            intdiv($n, 3),
            intdiv(2 * $n, 3),
        ]));
        $worst = 0.0;
        foreach ($rows as $row) {
            if ($row < 0 || $row >= $n) {
                continue;
            }
            $expected = $format->referenceRow(
                substr($weights, $row * $rowBytes, $rowBytes),
                $input,
                $k,
            );
            $actual = $result[$row];
            $error = abs(($actual - $expected) / ($expected !== 0.0 ? $expected : 1.0));
            if ($error > $worst) {
                $worst = $error;
            }
        }

        return $worst;
    }

    /** @param array{median: float, error: float, workers: list<float>} $measurement */
    private static function report(
        string $name,
        Format $format,
        int $k,
        int $n,
        int $threads,
        array $measurement,
        int $weightBytes,
        float $operations,
    ): void {
        $median = $measurement['median'];
        printf(
            "gemv: %-8s threads=%-2d median=%8.3f ms, %6.2f GMAC/s, %6.2f GB/s\n",
            $name,
            $threads,
            $median * 1000.0,
            $operations / $median / 1e9,
            $weightBytes / $median / 1e9,
        );
    }

    public static function benchmark(
        Format $format,
        int $k,
        int $n,
        int $threads,
        ?string $pin = null,
        int $warmup = 3,
        int $iterations = 20,
    ): void {
        $format->validateColumns($k);
        if ($n <= 0 || $threads <= 0 || $threads > $n || $warmup < 0 || $iterations <= 0) {
            throw new ValueError('n must be positive, threads between 1 and n, iterations positive');
        }

        $cpus = [];
        if ($pin !== null) {
            if (!CpuAffinity::isAvailable()) {
                fwrite(STDERR, "gemv: pinning off: FFI is unavailable\n");
            } else {
                $cpus = CpuAffinity::physicalCpus($pin);
            }
        }

        $started = hrtime(true);
        $weights = $format->generate($n, $k, Matrix::WEIGHT_SEED);
        $input = Matrix::fillRange($k, Matrix::INPUT_SEED);
        $generated = (hrtime(true) - $started) / 1e9;
        $weightBytes = strlen($weights);
        $operations = (float) $k * $n;

        $single = self::measure($format, $weights, $input, $k, $n, 1, $cpus, $warmup, $iterations);
        $parallel = self::measure($format, $weights, $input, $k, $n, $threads, $cpus, $warmup, $iterations);
        $speedup = $single['median'] / $parallel['median'];

        printf("gemv: format=%s m=1 k=%d n=%d\n", $format->name(), $k, $n);
        printf(
            "gemv: warmup=%d iterations=%d, generate=%.3f s, pinning=%s cpus=%s\n",
            $warmup,
            $iterations,
            $generated,
            $cpus === [] ? 'off' : 'on',
            $cpus === [] ? '-' : implode(',', $cpus),
        );
        self::report('single', $format, $k, $n, 1, $single, $weightBytes, $operations);
        self::report('parallel', $format, $k, $n, $threads, $parallel, $weightBytes, $operations);
        printf(
            "gemv: speedup %.2fx, efficiency %.1f%%, weights=%d bytes (%.2f bits/weight)\n",
            $speedup,
            $speedup / $threads * 100.0,
            $weightBytes,
            $weightBytes * 8.0 / $operations,
        );
        printf(
            "gemv: php=%s jit=%s cpu=%s\n",
            PHP_VERSION,
            System::jit(),
            System::cpuName(),
        );
        $worst = max($single['error'], $parallel['error']);
        printf(
            "gemv: sampled check single=%.3e parallel=%.3e (%s)\n",
            $single['error'],
            $parallel['error'],
            $worst < self::ERROR_TOLERANCE ? 'ok' : 'MISMATCH',
        );
        if ($worst >= self::ERROR_TOLERANCE) {
            throw new ValueError('gemv result mismatch');
        }
    }
}
