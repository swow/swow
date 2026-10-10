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

require_once __DIR__ . '/CommandObserver.php';

use MongoDB\BSON\Int64;
use MongoDB\Driver\BulkWrite;
use MongoDB\Driver\Command;
use MongoDB\Driver\Manager;
use MongoDB\Driver\Query;
use RuntimeException;
use stdClass;
use Swow\Channel;
use Swow\Coroutine;
use Throwable;

use function count;
use function extension_loaded;
use function function_exists;
use function in_array;
use function ini_get;
use function MongoDB\Driver\Monitoring\addSubscriber;
use function MongoDB\Driver\Monitoring\removeSubscriber;
use function sprintf;

use const JSON_THROW_ON_ERROR;
use const PHP_VERSION;

final class Benchmark
{
    /** @var list<float> */
    private array $latencies = [];
    /** @var list<Throwable> */
    private array $errors = [];
    private int $next = 0;
    private int $completed = 0;
    private int $warmedClients = 0;
    private float $maximumTimerLag = 0;
    private int $timerTicksDuringWorkload = 0;
    private bool $startingWorkers = true;
    private float $workerStartupMs = 0;
    private ?float $maximumSteadyTimerLag = null;
    private ?float $timerLagCpuMs = null;
    private string $collection;
    private ?stdClass $topologyVersion = null;
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
        if ($this->scenario === 'await') {
            // Awaitable hello is a real server command that waits for a
            // topology change or its deadline, without JavaScript execution.
            $rows = $manager->executeCommand('admin', new Command([
                'hello' => 1, 'topologyVersion' => $this->topologyVersion,
                'maxAwaitTimeMS' => new Int64((string) $this->delayMs),
            ]))->toArray();
            if (count($rows) !== 1 || (float) $rows[0]->ok !== 1.0 ||
                (string) $rows[0]->topologyVersion->processId !== (string) $this->topologyVersion->processId ||
                (string) $rows[0]->topologyVersion->counter !== (string) $this->topologyVersion->counter) {
                throw new RuntimeException('Awaitable hello returned an unexpected topology');
            }
            return;
        }
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
                if (in_array($this->scenario, ['delay', 'await'], true)) {
                    // Warm each native connection without charging a separate
                    // server-side sleep for every client before measurement.
                    $manager->executeCommand('admin', new Command(['ping' => 1]))->toArray();
                } else {
                    $this->operation($manager, $i);
                }
            }
            $this->warmedClients++;
        }
        if ($this->scenario === 'await') {
            $hello = $this->managers[0]->executeCommand('admin', new Command(['hello' => 1]))->toArray()[0];
            if (!isset($hello->topologyVersion)) {
                throw new RuntimeException('The server does not support awaitable hello');
            }
            $this->topologyVersion = $hello->topologyVersion;
            $this->topologyVersion->counter = new Int64((string) $this->topologyVersion->counter);
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

    /** @return array<string, int|float|string|bool|null> */
    public function run(): array
    {
        $commandName = $this->scenario === 'await' ? 'hello' : 'find';
        $observer = in_array($this->scenario, ['delay', 'await', 'query'], true) ? new CommandObserver($commandName) : null;
        try {
            $this->prepare();
            if ($observer !== null) {
                addSubscriber($observer);
            }
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
                        $startedDuringStartup = $this->startingWorkers;
                        $cpuBeforeTick = self::cpuSeconds();
                        usleep(1000);
                        if ($this->completed < $this->operations) {
                            $this->timerTicksDuringWorkload++;
                        }
                        $lag = (hrtime(true) - $before) / 1e6 - 1;
                        // Launching thousands of workers is itself PHP work.
                        // Retain the total lag and also measure later intervals.
                        if (!$startedDuringStartup) {
                            $this->maximumSteadyTimerLag = max($this->maximumSteadyTimerLag ?? 0, $lag);
                        }
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
                $this->workerStartupMs = (hrtime(true) - $start) / 1e6;
                $this->startingWorkers = false;
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
            $commands = $observer?->statistics() ?? [];
            if ($observer !== null && ($commands[$commandName . '_pending'] !== 0 ||
                $commands[$commandName . '_failed'] !== 0 || $commands[$commandName . '_completed'] !== $this->operations)) {
                throw new RuntimeException('Native command monitoring did not confirm the complete workload');
            }
            if ($this->scenario === 'insert') {
                $rows = $this->managers[0]->executeCommand($this->database, new Command(['count' => $this->collection]))->toArray();
                if ((int) $rows[0]->n !== 256 + $this->concurrency * $this->warmup + $this->operations) {
                    throw new RuntimeException('Final inserted document count is invalid');
                }
            }
            sort($this->latencies);
            $build = $this->managers[0]->executeCommand('admin', new Command(['buildInfo' => 1]))->toArray()[0];
            return array_merge([
                'mode' => $this->mode, 'clients' => $this->clients, 'scenario' => $this->scenario,
                'concurrency' => $this->concurrency, 'operations' => $this->completed,
                'warmup_per_client' => $this->warmup, 'delay_ms' => in_array($this->scenario, ['delay', 'await'], true) ? $this->delayMs : 0,
                'warmup_operation' => in_array($this->scenario, ['delay', 'await'], true) ? 'ping' : $this->scenario,
                'coroutines_enabled' => (bool) ini_get('swow.enable'),
                'mongodb_hook' => extension_loaded('swow') && \Swow\Extension::isBuiltWith('mongodb') && (bool) ini_get('swow.enable'),
                'elapsed_s' => $elapsed, 'operations_per_s' => $this->completed / $elapsed,
                'latency_p50_ms' => $this->percentile(0.50), 'latency_p95_ms' => $this->percentile(0.95),
                'latency_p99_ms' => $this->percentile(0.99), 'client_cpu_s' => $cpu,
                'peak_php_memory_bytes' => memory_get_peak_usage(true),
                'timer_max_lag_ms' => $this->mode === 'async' ? $this->maximumTimerLag : null,
                'worker_startup_ms' => $this->mode === 'async' ? $this->workerStartupMs : null,
                'timer_steady_max_lag_ms' => $this->mode === 'async' ? $this->maximumSteadyTimerLag : null,
                'timer_ticks_during_workload' => $this->mode === 'async' ? $this->timerTicksDuringWorkload : null,
                'timer_max_lag_client_cpu_ms' => $this->mode === 'async' ? $this->timerLagCpuMs : null,
                'gc_runs' => $gcAfter['runs'] - $gcBefore['runs'],
                'gc_collector_s' => isset($gcAfter['collector_time']) ? $gcAfter['collector_time'] - $gcBefore['collector_time'] : null,
                'php' => PHP_VERSION, 'mongodb_driver' => phpversion('mongodb'),
                'mongodb_server' => $build->version, 'uri_hash' => hash('sha256', $this->uri),
            ], $commands);
        } catch (Throwable $exception) {
            throw new RuntimeException(sprintf(
                'Clients warmed: %d/%d; operations completed: %d/%d; commands: %s; %s',
                $this->warmedClients, $this->concurrency, $this->completed, $this->operations,
                json_encode($observer?->statistics() ?? [], JSON_THROW_ON_ERROR), $exception->getMessage()
            ), 0, $exception);
        } finally {
            if ($observer !== null) {
                removeSubscriber($observer);
            }
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
