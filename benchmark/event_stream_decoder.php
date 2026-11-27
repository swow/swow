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

// Measures EventStreamDecoder on streams that end with one long data line.
// Usage: php benchmark/event_stream_decoder.php [repeat] (add -d extension=swow if the extension is not enabled)

use Swow\Psr7\Message\BufferStream;
use Swow\Psr7\Psr7;

require __DIR__ . '/../vendor/autoload.php';

$repeat = max(1, (int) ($argv[1] ?? 5));
$shortEvents = 1000;

/** Builds a stream of short delta events followed by one event whose data line has $longLineBytes bytes. */
$buildWire = static function (int $longLineBytes) use ($shortEvents): string {
    $wire = '';
    for ($i = 0; $i < $shortEvents; $i++) {
        $wire .= "event: delta\ndata: {\"index\":{$i},\"delta\":\"token\"}\n\n";
    }
    if ($longLineBytes > 0) {
        $wire .= "event: completed\ndata: " . str_repeat('x', $longLineBytes) . "\n\n";
    }
    return $wire;
};

/** Returns the median decode time in milliseconds and the number of decoded events. */
$measure = static function (string $wire, int $readSize) use ($repeat): array {
    $times = [];
    $count = 0;
    for ($run = 0; $run <= $repeat; $run++) {
        $start = hrtime(true);
        $count = 0;
        foreach (Psr7::readEventStream(new BufferStream($wire), $readSize) as $event) {
            $count++;
        }
        $elapsed = (hrtime(true) - $start) / 1e6;
        if ($run > 0) { // The first run is a warm-up.
            $times[] = $elapsed;
        }
    }
    sort($times);
    return [$times[intdiv(count($times), 2)], $count];
};

echo sprintf("%-10s %-14s %-12s %-8s %s\n", 'readSize', 'long line', 'median', 'events', 'x previous');
foreach ([64, 8192] as $readSize) {
    $previous = null;
    foreach ([0, 128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024] as $longLineBytes) {
        [$ms, $count] = $measure($buildWire($longLineBytes), $readSize);
        // The ratio to the previous row shows how the cost grows when the line length doubles.
        $ratio = ($previous !== null && $longLineBytes > 128 * 1024) ? sprintf('%.2f', $ms / $previous) : '-';
        echo sprintf("%-10d %-14s %-12s %-8d %s\n", $readSize, $longLineBytes === 0 ? 'none' : ($longLineBytes / 1024) . ' KiB', sprintf('%.2f ms', $ms), $count, $ratio);
        $previous = $longLineBytes > 0 ? $ms : null;
    }
}
