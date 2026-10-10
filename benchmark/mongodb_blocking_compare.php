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

$options = getopt('', ['uri:', 'database:', 'mongodb-so:', 'hooked-swow-so:', 'unhooked-swow-so:', 'delay-ms:', 'tick-ms:', 'repeats:', 'help']);
if (isset($options['help']) || !isset($options['uri'], $options['mongodb-so'], $options['hooked-swow-so'], $options['unhooked-swow-so'])) {
    echo "Compare MongoDB blocking with Swow enabled in BOTH processes.\n",
    "--uri=URI --mongodb-so=PATH --hooked-swow-so=PATH --unhooked-swow-so=PATH\n",
    "Build both Swow modules from the same source, differing only in --enable/disable-swow-mongodb.\n",
    "--delay-ms=1000 --tick-ms=5 --repeats=3 --database=swow_benchmark\n";
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
    $positive = static function (string $key, int $default) use ($string): int {
        $value = filter_var($string($key, (string) $default), FILTER_VALIDATE_INT, ['options' => ['min_range' => 1]]);
        if ($value === false) {
            throw new InvalidArgumentException("--{$key} must be a positive integer");
        }
        return $value;
    };
    $mongodb = realpath($string('mongodb-so', ''));
    $modules = ['hooked' => realpath($string('hooked-swow-so', '')), 'unhooked' => realpath($string('unhooked-swow-so', ''))];
    if ($mongodb === false || in_array(false, $modules, true) || $modules['hooked'] === $modules['unhooked']) {
        throw new InvalidArgumentException('Supply one MongoDB module and two distinct Swow modules');
    }
    $repeats = $positive('repeats', 3);
    $samples = [];
    for ($repeat = 0; $repeat < $repeats; $repeat++) {
        $order = $repeat % 2 === 0 ? ['unhooked', 'hooked'] : ['hooked', 'unhooked'];
        foreach ($order as $mode) {
            $command = [PHP_BINARY, '-n', '-d', 'extension=' . $mongodb, '-d', 'extension=' . $modules[$mode],
                '-d', 'swow.enable=1', __DIR__ . '/mongodb_blocking.php', '--uri=' . $string('uri', ''),
                '--database=' . $string('database', 'swow_benchmark'), '--delay-ms=' . $positive('delay-ms', 1000),
                '--tick-ms=' . $positive('tick-ms', 5)];
            $process = proc_open($command, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
            if (!is_resource($process)) {
                throw new RuntimeException('Could not start probe process');
            }
            fclose($pipes[0]);
            $output = stream_get_contents($pipes[1]);
            $error = stream_get_contents($pipes[2]);
            fclose($pipes[1]);
            fclose($pipes[2]);
            $status = proc_close($process);
            if ($status !== 0 || $error !== '') {
                throw new RuntimeException("Probe failed ({$status}): {$error}{$output}");
            }
            $sample = json_decode($output, true, flags: JSON_THROW_ON_ERROR);
            if (!$sample['coroutines_enabled'] || $sample['ticks_before_query'] === 0 ||
                $sample['mongodb_hook'] !== ($mode === 'hooked') || !$sample['result_valid']) {
                throw new RuntimeException('The loaded modules do not match the requested comparison');
            }
            if (($mode === 'hooked' && $sample['ticks_during_query'] === 0) ||
                ($mode === 'unhooked' && $sample['ticks_during_query'] !== 0)) {
                throw new RuntimeException('The blocking/nonblocking assertion failed');
            }
            $sample['mode'] = $mode;
            $sample['repeat'] = $repeat + 1;
            $samples[] = $sample;
            fwrite(STDERR, json_encode($sample, JSON_THROW_ON_ERROR) . PHP_EOL);
        }
    }
    echo json_encode(['samples' => $samples], JSON_THROW_ON_ERROR | JSON_PRETTY_PRINT), PHP_EOL;
} catch (Throwable $exception) {
    fwrite(STDERR, $exception::class . ': ' . $exception->getMessage() . PHP_EOL);
    exit(1);
}
