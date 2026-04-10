<?php

declare(strict_types=1);

namespace Swow\Http\Protocol;

use function array_slice;
use function strlen;

final class H2HpackDynamicTable
{
    private const ENTRY_OVERHEAD = 32;

    /**
     * @var list<array{name: string, value: string}>
     */
    private array $entries = [];

    private int $head = 0;

    private int $tail = 0;

    private int $size = 0;

    public function __construct(private int $maxSize = 4096)
    {
    }

    public function insert(string $name, string $value): void
    {
        $entrySize = strlen($name) + strlen($value) + self::ENTRY_OVERHEAD;
        if ($entrySize > $this->maxSize) {
            $this->entries = [];
            $this->head = 0;
            $this->tail = 0;
            $this->size = 0;
            return;
        }

        while (($this->size + $entrySize) > $this->maxSize && $this->head < $this->tail) {
            $this->evict();
        }

        $this->entries[] = ['name' => $name, 'value' => $value];
        $this->tail++;
        $this->size += $entrySize;

        if ($this->head > 256) {
            $this->entries = array_slice($this->entries, $this->head);
            $this->tail -= $this->head;
            $this->head = 0;
        }
    }

    public function setMaxSize(int $maxSize): void
    {
        $this->maxSize = $maxSize;
        while ($this->size > $this->maxSize && $this->head < $this->tail) {
            $this->evict();
        }
    }

    /**
     * @return array{name: string, value: string}|null
     */
    public function get(int $index): ?array
    {
        $logicalCount = $this->tail - $this->head;
        if ($index >= $logicalCount) {
            return null;
        }

        $realIndex = $this->tail - 1 - $index;
        return $this->entries[$realIndex];
    }

    public function count(): int
    {
        return $this->tail - $this->head;
    }

    private function evict(): void
    {
        if ($this->head >= $this->tail) {
            return;
        }

        $entry = $this->entries[$this->head];
        $this->head++;
        $this->size -= strlen($entry['name']) + strlen($entry['value']) + self::ENTRY_OVERHEAD;
    }
}
