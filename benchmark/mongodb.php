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

require __DIR__ . '/mongodb/Benchmark.php';

use Swow\Benchmark\MongoDB\Benchmark;

$options = getopt('', ['uri:', 'database:', 'mode:', 'mongodb-hook:', 'clients:', 'scenario:', 'concurrency:', 'operations:', 'duration-ms:', 'repeats:', 'warmup:', 'delay-ms:', 'help']);
if (isset($options['help']) || !isset($options['uri'])) {
    echo "MongoDB benchmark (creates and drops a temporary collection in swow_benchmark).\n",
    "--uri=URI --mode=sync|async --clients=independent|shared --scenario=ping|query|insert|delay|await\n",
    "--concurrency=16 --operations=1000 --warmup=10 --delay-ms=10 --database=swow_benchmark\n",
    "Use -d swow.enable=0 for sync, and -d swow.enable=1 for async. JSON is written to stdout.\n";
    echo "--mongodb-hook=on|off selects the expected MongoDB adapter build (default on).\n";
    echo "--duration-ms=10000 stops new commands at the deadline and includes completion of all in-flight commands.\n",
    "--repeats=3 reuses warmed native clients; the default single sample retains the original JSON format.\n";
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
    $number = static function (string $key, int $default) use ($string): int {
        $value = filter_var($string($key, (string) $default), FILTER_VALIDATE_INT, ['options' => ['min_range' => 1]]);
        if ($value === false) {
            throw new InvalidArgumentException("--{$key} must be a positive integer");
        }
        return $value;
    };
    $mode = $string('mode', 'async');
    $clients = $string('clients', 'independent');
    $scenario = $string('scenario', 'query');
    $hook = $string('mongodb-hook', 'on');
    if (!in_array($mode, ['sync', 'async'], true) || !in_array($clients, ['independent', 'shared'], true) ||
        !in_array($scenario, ['ping', 'query', 'insert', 'delay', 'await'], true) || !in_array($hook, ['on', 'off'], true)) {
        throw new InvalidArgumentException('Invalid mode, clients or scenario');
    }
    $enabled = extension_loaded('swow') && (bool) ini_get('swow.enable');
    if (($mode === 'async') !== $enabled || ($enabled && Swow\Extension::isBuiltWith('mongodb') !== ($hook === 'on'))) {
        throw new RuntimeException('The requested mode does not match the loaded Swow configuration');
    }
    $durationMs = isset($options['duration-ms']) ? $number('duration-ms', 10000) : null;
    $benchmark = new Benchmark($string('uri', ''), $mode, $clients, $scenario,
        $number('concurrency', 16), $number('operations', $durationMs === null ? 1000 : PHP_INT_MAX), $number('warmup', 10), $number('delay-ms', 10),
        $string('database', 'swow_benchmark'), $durationMs);
    $repeats = $number('repeats', 1);
    $samples = [];
    for ($repeat = 0; $repeat < $repeats; $repeat++) {
        $samples[] = $benchmark->run();
        if ($repeats > 1) {
            fwrite(STDERR, json_encode(['repeat' => $repeat + 1, 'sample' => $samples[$repeat]], JSON_THROW_ON_ERROR) . PHP_EOL);
        }
    }
    $result = $repeats === 1 ? $samples[0] : ['samples' => $samples];
    echo json_encode($result, JSON_THROW_ON_ERROR | JSON_UNESCAPED_SLASHES), PHP_EOL;
} catch (Throwable $exception) {
    fwrite(STDERR, $exception::class . ': ' . $exception->getMessage() . PHP_EOL);
    exit(1);
}
