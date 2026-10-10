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

$options = getopt('', ['uri:', 'database:', 'mongodb-so:', 'hooked-swow-so:', 'unhooked-swow-so:',
    'clients:', 'scenario:', 'hook-modes:', 'concurrency:', 'operations:', 'delay-ms:', 'repeats:', 'help']);
if (isset($options['help']) || !isset($options['uri'], $options['mongodb-so'], $options['hooked-swow-so'], $options['unhooked-swow-so'])) {
    echo "Slow MongoDB throughput comparison; Swow coroutines remain enabled in both groups.\n",
    "--uri=URI --mongodb-so=PATH --hooked-swow-so=PATH --unhooked-swow-so=PATH\n",
    "--clients=independent|shared --concurrency=1,4,16,64 --operations=64 --delay-ms=250 --repeats=3\n",
    "--scenario=delay|await|query (default delay): server JavaScript sleep, awaitable hello, or indexed find.\n",
    "--hook-modes=unhooked,hooked (default); select hooked alone for a capacity sweep.\n",
    "Uses a real MongoDB server and temporary collections in swow_benchmark.\n";
    exit(isset($options['help']) ? 0 : 1);
}
try {
    $string = static function (string $key, string $default) use ($options): string {
        $value = $options[$key] ?? $default;
        if (!is_string($value) || $value === '') {
            throw new InvalidArgumentException("Invalid --{$key}");
        }
        return $value;
    };
    $positive = static function (string $value): int {
        $number = filter_var($value, FILTER_VALIDATE_INT, ['options' => ['min_range' => 1]]);
        if ($number === false) {
            throw new InvalidArgumentException('Counts and delay must be positive integers');
        }
        return $number;
    };
    $mongodb = realpath($string('mongodb-so', ''));
    $modules = ['hooked' => realpath($string('hooked-swow-so', '')), 'unhooked' => realpath($string('unhooked-swow-so', ''))];
    if ($mongodb === false || in_array(false, $modules, true) || $modules['hooked'] === $modules['unhooked']) {
        throw new InvalidArgumentException('Supply one MongoDB module and two distinct Swow modules');
    }
    $clients = $string('clients', 'independent');
    if (!in_array($clients, ['independent', 'shared'], true)) {
        throw new InvalidArgumentException('Invalid clients');
    }
    $scenario = $string('scenario', 'delay');
    if (!in_array($scenario, ['delay', 'await', 'query'], true)) {
        throw new InvalidArgumentException('Invalid scenario');
    }
    $commandName = $scenario === 'await' ? 'hello' : 'find';
    $modes = explode(',', $string('hook-modes', 'unhooked,hooked'));
    if (count(array_unique($modes)) !== count($modes) || array_diff($modes, ['unhooked', 'hooked']) !== []) {
        throw new InvalidArgumentException('Invalid hook modes');
    }
    $counts = array_map($positive, explode(',', $string('concurrency', '1,4,16,64')));
    $operations = $positive($string('operations', '64'));
    if (max($counts) > $operations) {
        throw new InvalidArgumentException('Operations must cover every requested client');
    }
    $repeats = $positive($string('repeats', '3'));
    $samples = [];
    for ($repeat = 0; $repeat < $repeats; $repeat++) {
        $order = $repeat % 2 === 0 ? $modes : array_reverse($modes);
        $roundCounts = $repeat % 2 === 0 ? $counts : array_reverse($counts);
        foreach ($roundCounts as $count) {
            foreach ($order as $mode) {
                $command = [PHP_BINARY, '-n', '-d', 'extension=' . $mongodb, '-d', 'extension=' . $modules[$mode],
                    '-d', 'swow.enable=1', __DIR__ . '/mongodb.php', '--uri=' . $string('uri', ''),
                    '--database=' . $string('database', 'swow_benchmark'), '--mode=async',
                    '--mongodb-hook=' . ($mode === 'hooked' ? 'on' : 'off'), '--clients=' . $clients,
                    '--scenario=' . $scenario, '--concurrency=' . $count, '--operations=' . $operations,
                    '--warmup=1', '--delay-ms=' . $positive($string('delay-ms', '250'))];
                $process = proc_open($command, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
                if (!is_resource($process)) {
                    throw new RuntimeException('Could not start benchmark');
                }
                fclose($pipes[0]);
                $output = stream_get_contents($pipes[1]);
                $error = stream_get_contents($pipes[2]);
                fclose($pipes[1]);
                fclose($pipes[2]);
                $status = proc_close($process);
                if ($status !== 0 || $error !== '') {
                    throw new RuntimeException("Benchmark failed ({$status}): {$error}{$output}");
                }
                $sample = json_decode($output, true, flags: JSON_THROW_ON_ERROR);
                if (!$sample['coroutines_enabled'] || $sample['mongodb_hook'] !== ($mode === 'hooked') ||
                    $sample['operations'] !== $operations || $sample[$commandName . '_completed'] !== $operations ||
                    $sample[$commandName . '_failed'] !== 0 || $sample[$commandName . '_pending'] !== 0) {
                    throw new RuntimeException('The loaded modules or measured workload do not match the comparison');
                }
                $sample['hook_mode'] = $mode;
                $sample['repeat'] = $repeat + 1;
                $samples[] = $sample;
                fwrite(STDERR, json_encode($sample, JSON_THROW_ON_ERROR) . PHP_EOL);
            }
        }
    }
    /** @param list<int|float> $values */
    $median = static function (array $values): float {
        sort($values);
        $middle = intdiv(count($values), 2);
        return count($values) % 2 ? (float) $values[$middle] : ($values[$middle - 1] + $values[$middle]) / 2;
    };
    $groups = [];
    foreach ($samples as $sample) {
        $groups[$sample['concurrency'] . '/' . $sample['hook_mode']][] = $sample;
    }
    $summary = [];
    foreach ($groups as $key => $rows) {
        [$count, $mode] = explode('/', $key);
        $ratios = [];
        if ($mode === 'hooked' && isset($groups[$count . '/unhooked'])) {
            $baseline = array_column($groups[$count . '/unhooked'], 'operations_per_s', 'repeat');
            foreach ($rows as $row) {
                $ratios[] = $row['operations_per_s'] / $baseline[$row['repeat']];
            }
        }
        $summary[] = [
            'scenario' => $scenario, 'clients' => $clients, 'concurrency' => (int) $count, 'hook_mode' => $mode, 'samples' => count($rows),
            'median_operations_per_s' => $median(array_column($rows, 'operations_per_s')),
            'min_operations_per_s' => min(array_column($rows, 'operations_per_s')),
            'max_operations_per_s' => max(array_column($rows, 'operations_per_s')),
            'median_elapsed_s' => $median(array_column($rows, 'elapsed_s')),
            'median_paired_speedup' => $mode === 'unhooked' ? 1.0 : ($ratios !== [] ? $median($ratios) : null),
            'median_latency_p95_ms' => $median(array_column($rows, 'latency_p95_ms')),
            'median_latency_p99_ms' => $median(array_column($rows, 'latency_p99_ms')),
            'min_' . $commandName . '_max_in_flight' => min(array_column($rows, $commandName . '_max_in_flight')),
            'max_' . $commandName . '_max_in_flight' => max(array_column($rows, $commandName . '_max_in_flight')),
            'median_timer_max_lag_ms' => $median(array_column($rows, 'timer_max_lag_ms')),
            'worst_timer_max_lag_ms' => max(array_column($rows, 'timer_max_lag_ms')),
            'median_timer_ticks_during_workload' => $median(array_column($rows, 'timer_ticks_during_workload')),
        ];
    }
    echo json_encode(['samples' => $samples, 'summary' => $summary], JSON_THROW_ON_ERROR | JSON_PRETTY_PRINT), PHP_EOL;
} catch (Throwable $exception) {
    fwrite(STDERR, $exception::class . ': ' . $exception->getMessage() . PHP_EOL);
    exit(1);
}
