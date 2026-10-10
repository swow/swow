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

$options = getopt('', ['uri:', 'database:', 'mongodb-so:', 'swow-so:', 'concurrency:', 'operations:', 'delay-operations:', 'delay-ms:', 'repeats:', 'help']);
if (isset($options['help']) || !isset($options['uri'], $options['mongodb-so'], $options['swow-so'])) {
    echo "MongoDB benchmark matrix; invokes mongodb.php with fresh, isolated PHP processes.\n",
    "--uri=URI --mongodb-so=PATH --swow-so=PATH --concurrency=1,4,16,64\n",
    "--operations=20000 --delay-operations=160 --delay-ms=10 --repeats=3\n",
    "All queries use a temporary collection in swow_benchmark. JSON is written to stdout.\n";
    exit(isset($options['help']) ? 0 : 1);
}
try {
    $value = static function (string $key, string $default) use ($options): string {
        $value = $options[$key] ?? $default;
        if (!is_string($value) || $value === '') {
            throw new InvalidArgumentException("Invalid --{$key}");
        }
        return $value;
    };
    $positive = static function (string $value): int {
        $result = filter_var($value, FILTER_VALIDATE_INT, ['options' => ['min_range' => 1]]);
        if ($result === false) {
            throw new InvalidArgumentException('Counts must be positive integers');
        }
        return $result;
    };
    $uri = $value('uri', '');
    $mongodb = realpath($value('mongodb-so', ''));
    $swow = realpath($value('swow-so', ''));
    if ($mongodb === false || $swow === false) {
        throw new InvalidArgumentException('Extension files do not exist');
    }
    $counts = array_map($positive, explode(',', $value('concurrency', '1,4,16,64')));
    $repeats = $positive($value('repeats', '3'));
    $rows = [];
    for ($repeat = 0; $repeat < $repeats; $repeat++) {
        $modes = $repeat % 2 === 0 ? ['sync', 'async'] : ['async', 'sync'];
        $roundCounts = $repeat % 2 === 0 ? $counts : array_reverse($counts);
        foreach (['ping', 'query', 'insert', 'delay'] as $scenario) {
            foreach ($roundCounts as $count) {
                foreach (['independent', 'shared'] as $clients) {
                    foreach ($modes as $mode) {
                        $command = [PHP_BINARY, '-n', '-d', 'extension=' . $mongodb, '-d', 'extension=' . $swow,
                            '-d', 'swow.enable=' . ($mode === 'async' ? '1' : '0'), __DIR__ . '/mongodb.php',
                            '--uri=' . $uri, '--database=' . $value('database', 'swow_benchmark'),
                            '--mode=' . $mode, '--clients=' . $clients, '--scenario=' . $scenario,
                            '--concurrency=' . $count,
                            '--operations=' . $positive($value($scenario === 'delay' ? 'delay-operations' : 'operations', $scenario === 'delay' ? '160' : '20000')),
                            '--warmup=' . ($scenario === 'delay' ? '1' : '5'), '--delay-ms=' . $positive($value('delay-ms', '10'))];
                        // An argument array avoids shell quoting and leaves the
                        // target PHP configuration independent of the runner.
                        $process = proc_open($command, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
                        if (!is_resource($process)) {
                            throw new RuntimeException('Could not start benchmark process');
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
                        $row = json_decode($output, true, flags: JSON_THROW_ON_ERROR);
                        $row['repeat'] = $repeat + 1;
                        $rows[] = $row;
                        // Keep progress and partial evidence if a later sample fails.
                        fwrite(STDERR, json_encode($row, JSON_THROW_ON_ERROR) . PHP_EOL);
                    }
                }
            }
        }
    }
    $groups = [];
    foreach ($rows as $row) {
        $key = implode('/', [$row['scenario'], $row['clients'], $row['concurrency'], $row['mode']]);
        $groups[$key][] = $row;
    }
    /** @param list<int|float> $values */
    $median = static function (array $values): float {
        sort($values);
        $middle = intdiv(count($values), 2);
        return count($values) % 2 ? (float) $values[$middle] : ($values[$middle - 1] + $values[$middle]) / 2;
    };
    $summary = [];
    foreach ($groups as $key => $samples) {
        [$scenario, $clients, $count, $mode] = explode('/', $key);
        $pairedSpeedups = [];
        if ($mode === 'async') {
            $sync = array_column($groups["{$scenario}/{$clients}/{$count}/sync"], 'operations_per_s', 'repeat');
            foreach ($samples as $sample) {
                $pairedSpeedups[] = $sample['operations_per_s'] / $sync[$sample['repeat']];
            }
        }
        $summary[] = [
            'scenario' => $scenario, 'clients' => $clients, 'concurrency' => (int) $count, 'mode' => $mode,
            'samples' => count($samples),
            'median_operations_per_s' => $median(array_column($samples, 'operations_per_s')),
            'min_operations_per_s' => min(array_column($samples, 'operations_per_s')),
            'max_operations_per_s' => max(array_column($samples, 'operations_per_s')),
            'median_paired_speedup' => $mode === 'async' ? $median($pairedSpeedups) : 1.0,
            'median_elapsed_s' => $median(array_column($samples, 'elapsed_s')),
            'median_latency_p95_ms' => $median(array_column($samples, 'latency_p95_ms')),
            'median_latency_p99_ms' => $median(array_column($samples, 'latency_p99_ms')),
            'median_client_cpu_s' => $samples[0]['client_cpu_s'] !== null ? $median(array_column($samples, 'client_cpu_s')) : null,
            'worst_timer_lag_ms' => $mode === 'async' ? max(array_column($samples, 'timer_max_lag_ms')) : null,
        ];
    }
    echo json_encode(['samples' => $rows, 'summary' => $summary], JSON_THROW_ON_ERROR | JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES), PHP_EOL;
} catch (Throwable $exception) {
    fwrite(STDERR, $exception::class . ': ' . $exception->getMessage() . PHP_EOL);
    exit(1);
}
