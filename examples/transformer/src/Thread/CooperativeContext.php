<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Thread;

use RuntimeException;
use Swow\SharedBuffer;
use Swow\Sync\ThreadBarrier;

/*
 * Per-worker state of the cooperative execution.
 *
 * Every worker runs the same forward code and holds the same inputs. Only the
 * linear outputs are shared: a worker writes its row range into one of the two
 * output buffers, waits at the op barrier, then reads the full result back.
 * Two buffers let a fast worker start the next op while a slow worker still
 * reads the previous result.
 */
final class CooperativeContext
{
    private static ?self $current = null;

    private int $opIndex = 0;

    public function __construct(
        public int $rank,
        public int $threads,
        public SharedBuffer $outputA,
        public SharedBuffer $outputB,
        public SharedBuffer $logits,
        public SharedBuffer $control,
        public ThreadBarrier $opBarrier,
        public ThreadBarrier $tokenBarrier,
    ) {
    }

    public static function set(?self $context): void
    {
        self::$current = $context;
    }

    public static function current(): ?self
    {
        return self::$current;
    }

    public static function active(): bool
    {
        return self::$current !== null;
    }

    public static function get(): self
    {
        if (self::$current === null) {
            throw new RuntimeException('Cooperative context is not initialized');
        }

        return self::$current;
    }

    public function outputBuffer(int $index): SharedBuffer
    {
        return ($index & 1) === 0 ? $this->outputA : $this->outputB;
    }

    public function opIndex(): int
    {
        return $this->opIndex;
    }

    public function advanceOp(): void
    {
        $this->opIndex++;
    }
}
