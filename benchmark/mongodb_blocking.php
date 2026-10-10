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

require __DIR__ . '/mongodb/BlockingProbe.php';

use Swow\Benchmark\MongoDB\BlockingProbe;

$options = getopt('', ['uri:', 'database:', 'delay-ms:', 'tick-ms:', 'help']);
if (isset($options['help']) || !isset($options['uri'])) {
    echo "MongoDB blocking probe; Swow must remain enabled, with or without the MongoDB adapter.\n",
    "--uri=URI --database=swow_benchmark --delay-ms=1000 --tick-ms=5\n",
    "Creates and drops a temporary collection; outputs JSON.\n";
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
        $number = filter_var($string($key, (string) $default), FILTER_VALIDATE_INT, ['options' => ['min_range' => 1]]);
        if ($number === false) {
            throw new InvalidArgumentException("--{$key} must be a positive integer");
        }
        return $number;
    };
    $result = (new BlockingProbe())->run($string('uri', ''), $string('database', 'swow_benchmark'),
        $positive('delay-ms', 1000), $positive('tick-ms', 5));
    echo json_encode($result, JSON_THROW_ON_ERROR), PHP_EOL;
} catch (Throwable $exception) {
    fwrite(STDERR, $exception::class . ': ' . $exception->getMessage() . PHP_EOL);
    exit(1);
}
