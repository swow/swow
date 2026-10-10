<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Benchmark\MongoDB;

use MongoDB\Driver\BulkWrite;
use MongoDB\Driver\Command;
use MongoDB\Driver\Manager;
use MongoDB\Driver\Query;
use RuntimeException;
use Swow\Channel;
use Swow\Coroutine;
use Throwable;

use function count;
use function function_exists;
use function sprintf;

use const PHP_VERSION;

final class Benchmark
{
    /** @var list<float> */
    private array $latencies = [];
    /** @var list<Throwable> */
    private array $errors = [];
    private int $next = 0;
    private int $completed = 0;
    private float $maximumTimerLag = 0;
    private ?float $timerLagCpuMs = null;
    private string $collection;
    /** @var list<Manager> */
    private array $managers = [];

    public function __construct(
        private string $uri,
        private string $mode,
        private string $clients,
        private string $scenario,
        private int $concurrency,
        private int $operations,
        private int $warmup,
        private int $delayMs,
        private string $database,
    ) {
        $this->collection = 'benchmark_' . bin2hex(random_bytes(12));
        for ($i = 0; $i < $concurrency; $i++) {
            $this->managers[] = new Manager($uri, [], ['disableClientPersistence' => $clients === 'independent']);
        }
    }

    private function operation(Manager $manager, int $index): void
    {
        if ($this->scenario === 'ping') {
            $rows = $manager->executeCommand('admin', new Command(['ping' => 1]))->toArray();
            if ((float) $rows[0]->ok !== 1.0) {
                throw new RuntimeException('Ping result is invalid');
            }
            return;
        }
        if ($this->scenario === 'insert') {
            $bulk = new BulkWrite();
            $bulk->insert(['value' => $index, 'payload' => str_repeat('x', 1024)]);
            if ($manager->executeBulkWrite($this->database . '.' . $this->collection, $bulk)->getInsertedCount() !== 1) {
                throw new RuntimeException('Insert was not acknowledged');
            }
            return;
        }
        $id = $index % 256;
        $filter = ['_id' => $id];
        if ($this->scenario === 'delay') {
            // Controlled server-side waiting is reported separately from normal
            // indexed queries. It must never stand in for their performance.
            $filter['$where'] = sprintf('function () { sleep(%d); return true; }', $this->delayMs);
        }
        $rows = $manager->executeQuery(
            $this->database . '.' . $this->collection,
            new Query($filter, ['limit' => 1, 'projection' => ['_id' => 1]])
        )->toArray();
        if (count($rows) !== 1 || $rows[0]->_id !== $id) {
            throw new RuntimeException('Query returned an unexpected document');
        }
    }

    private function prepare(): void
    {
        $bulk = new BulkWrite();
        for ($id = 0; $id < 256; $id++) {
            $bulk->insert(['_id' => $id, 'payload' => str_repeat('x', 1024)]);
        }
        $this->managers[0]->executeBulkWrite($this->database . '.' . $this->collection, $bulk);
        foreach ($this->managers as $manager) {
            for ($i = 0; $i < $this->warmup; $i++) {
                $this->operation($manager, $i);
            }
        }
    }

    private function worker(Manager $manager): void
    {
        try {
            // Allocation is atomic here: PHP switches only when an operation
            // suspends. Every mode executes exactly the same total workload.
            while ($this->next < $this->operations) {
                $index = $this->next++;
                $start = hrtime(true);
                $this->operation($manager, $index);
                $this->latencies[] = (hrtime(true) - $start) / 1e6;
                $this->completed++;
            }
        } catch (Throwable $exception) {
            $this->errors[] = $exception;
            $this->next = $this->operations;
        }
    }

    /** @return array<string, int|float|string|null> */
    public function run(): array
    {
        try {
            $this->prepare();
            $cpuBefore = self::cpuSeconds();
            $gcBefore = gc_status();
            $start = hrtime(true);
            if ($this->mode === 'sync') {
                // Round-robin the same warmed clients. This baseline has no
                // coroutine runtime; scheduler lag is deliberately null.
                for ($index = 0; $index < $this->operations; $index++) {
                    $before = hrtime(true);
                    $this->operation($this->managers[$index % $this->concurrency], $index);
                    $this->latencies[] = (hrtime(true) - $before) / 1e6;
                    $this->completed++;
                }
            } else {
                $done = new Channel($this->concurrency + 1);
                $stopped = false;
                $coroutines = [];
                $coroutines[] = Coroutine::run(function () use (&$stopped, $done): void {
                    while (!$stopped) {
                        $before = hrtime(true);
                        $cpuBeforeTick = self::cpuSeconds();
                        usleep(1000);
                        $lag = (hrtime(true) - $before) / 1e6 - 1;
                        if ($lag > $this->maximumTimerLag) {
                            $this->maximumTimerLag = $lag;
                            // Process CPU separates a busy cooperative loop
                            // from an interval mostly spent off the CPU.
                            $cpuAfterTick = self::cpuSeconds();
                            $this->timerLagCpuMs = $cpuBeforeTick !== null && $cpuAfterTick !== null
                                ? ($cpuAfterTick - $cpuBeforeTick) * 1000 : null;
                        }
                    }
                    $done->push(true);
                });
                foreach ($this->managers as $manager) {
                    $coroutines[] = Coroutine::run(function () use ($manager, $done): void {
                        $this->worker($manager);
                        $done->push(true);
                    });
                }
                foreach ($this->managers as $_manager) {
                    $done->pop();
                }
                $stopped = true;
            }
            $elapsed = (hrtime(true) - $start) / 1e9;
            $cpuAfter = self::cpuSeconds();
            $cpu = $cpuBefore !== null && $cpuAfter !== null ? $cpuAfter - $cpuBefore : null;
            $gcAfter = gc_status();
            if ($this->mode === 'async') {
                $done->pop();
            }
            if ($this->errors !== []) {
                throw $this->errors[0];
            }
            if ($this->completed !== $this->operations) {
                throw new RuntimeException('Measured workload is incomplete');
            }
            if ($this->scenario === 'insert') {
                $rows = $this->managers[0]->executeCommand($this->database, new Command(['count' => $this->collection]))->toArray();
                if ((int) $rows[0]->n !== 256 + $this->concurrency * $this->warmup + $this->operations) {
                    throw new RuntimeException('Final inserted document count is invalid');
                }
            }
            sort($this->latencies);
            $build = $this->managers[0]->executeCommand('admin', new Command(['buildInfo' => 1]))->toArray()[0];
            return [
                'mode' => $this->mode, 'clients' => $this->clients, 'scenario' => $this->scenario,
                'concurrency' => $this->concurrency, 'operations' => $this->completed,
                'warmup_per_client' => $this->warmup, 'delay_ms' => $this->scenario === 'delay' ? $this->delayMs : 0,
                'elapsed_s' => $elapsed, 'operations_per_s' => $this->completed / $elapsed,
                'latency_p50_ms' => $this->percentile(0.50), 'latency_p95_ms' => $this->percentile(0.95),
                'latency_p99_ms' => $this->percentile(0.99), 'client_cpu_s' => $cpu,
                'peak_php_memory_bytes' => memory_get_peak_usage(true),
                'timer_max_lag_ms' => $this->mode === 'async' ? $this->maximumTimerLag : null,
                'timer_max_lag_client_cpu_ms' => $this->mode === 'async' ? $this->timerLagCpuMs : null,
                'gc_runs' => $gcAfter['runs'] - $gcBefore['runs'],
                'gc_collector_s' => isset($gcAfter['collector_time']) ? $gcAfter['collector_time'] - $gcBefore['collector_time'] : null,
                'php' => PHP_VERSION, 'mongodb_driver' => phpversion('mongodb'),
                'mongodb_server' => $build->version, 'uri_hash' => hash('sha256', $this->uri),
            ];
        } finally {
            $this->managers[0]->executeCommand($this->database, new Command(['drop' => $this->collection]))->toArray();
        }
    }

    private function percentile(float $fraction): float
    {
        return $this->latencies[max(0, (int) ceil(count($this->latencies) * $fraction) - 1)];
    }

    private static function cpuSeconds(): ?float
    {
        if (!function_exists('getrusage')) {
            return null;
        }
        $usage = getrusage();
        if ($usage === false) {
            return null;
        }
        return $usage['ru_utime.tv_sec'] + $usage['ru_stime.tv_sec']
            + ($usage['ru_utime.tv_usec'] + $usage['ru_stime.tv_usec']) / 1e6;
    }
}
