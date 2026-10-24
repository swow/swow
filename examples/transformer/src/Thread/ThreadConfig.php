<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Thread;

use Swow\Examples\Gemm\Support\CpuAffinity;
use Swow\Thread;
use Throwable;

use function array_fill;
use function array_slice;
use function class_exists;
use function count;

final class ThreadConfig
{
    private static int $threads = 1;

    private static bool $pinned = true;

    public static function setThreads(int $threads): void
    {
        self::$threads = max(1, $threads);
    }

    public static function threads(): int
    {
        return self::$threads;
    }

    public static function setPinned(bool $pinned): void
    {
        self::$pinned = $pinned;
    }

    public static function pinned(): bool
    {
        return self::$pinned;
    }

    public static function defaultThreads(): int
    {
        if (!class_exists(Thread::class)) {
            return 1;
        }
        try {
            return max(1, count(CpuAffinity::physicalCpus('auto')));
        } catch (Throwable) {
            return 1;
        }
    }

    /** @return list<null|int> */
    public static function cpusFor(int $threads): array
    {
        if (!self::$pinned || !CpuAffinity::isAvailable()) {
            return array_fill(0, $threads, null);
        }
        try {
            $cpus = CpuAffinity::physicalCpus('auto');
        } catch (Throwable) {
            return array_fill(0, $threads, null);
        }
        if ($cpus === []) {
            return array_fill(0, $threads, null);
        }

        return array_slice($cpus, 0, $threads);
    }
}
