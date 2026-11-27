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

use Swow\Coroutine;

/**
 * Tells whether an operation lets other coroutines run while it waits.
 *
 * A ticker coroutine counts timer ticks during the operation. An operation that
 * blocks the thread for its whole duration leaves the count at about zero; one
 * that suspends only the calling coroutine lets it grow with the elapsed time.
 */
final class SchedulerProbe
{
    public const INTERVAL_MS = 10;

    /** @return array{result: mixed, ticks: int, elapsed_ms: float} */
    public static function measure(callable $operation): array
    {
        $ticks = 0;
        $running = true;
        // The ticker sees $running turn false within one interval and exits by itself.
        Coroutine::run(static function () use (&$ticks, &$running): void {
            while ($running) {
                usleep(self::INTERVAL_MS * 1000);
                if ($running) {
                    $ticks++;
                }
            }
        });
        $start = hrtime(true);
        try {
            $result = $operation();
        } finally {
            $elapsed = (hrtime(true) - $start) / 1e6;
            $running = false;
        }
        return ['result' => $result, 'ticks' => $ticks, 'elapsed_ms' => $elapsed];
    }

    /**
     * Run the operation and assert that the ticker kept running while it waited.
     * Only a quarter of the ideal tick count is required, so a loaded machine does not fail it.
     */
    public static function assertYields(callable $operation, float $minimumWaitMs, string $what): mixed
    {
        $measured = self::measure($operation);
        Assert::greaterThanEq($measured['elapsed_ms'], $minimumWaitMs * 0.8, "{$what} returned before it waited");
        $expected = (int) ($measured['elapsed_ms'] / self::INTERVAL_MS / 4);
        Assert::greaterThanEq($measured['ticks'], max(2, $expected), sprintf(
            '%s blocked the scheduler: %d ticks in %.1fms', $what, $measured['ticks'], $measured['elapsed_ms']
        ));
        return $measured['result'];
    }
}
