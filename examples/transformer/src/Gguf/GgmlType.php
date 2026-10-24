<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Gguf;

final class GgmlType
{
    public const F32 = 0;

    public const F16 = 1;

    public const Q4_0 = 2;

    public const Q8_0 = 8;

    public const BF16 = 30;

    /** @var array<int, array{0: int, 1: int}> type => [block size, bytes per block] */
    private const TYPES = [
        self::F32 => [1, 4],
        self::F16 => [1, 2],
        self::Q4_0 => [32, 18],
        self::Q8_0 => [32, 34],
        self::BF16 => [1, 2],
    ];

    public static function name(int $type): string
    {
        return match ($type) {
            self::F32 => 'F32',
            self::F16 => 'F16',
            self::Q4_0 => 'Q4_0',
            self::Q8_0 => 'Q8_0',
            self::BF16 => 'BF16',
            default => 'UNKNOWN(' . $type . ')',
        };
    }

    public static function blockSize(int $type): int
    {
        self::assertSupported($type);

        return self::TYPES[$type][0];
    }

    public static function bytesPerBlock(int $type): int
    {
        self::assertSupported($type);

        return self::TYPES[$type][1];
    }

    public static function byteSize(int $type, int $elements): int
    {
        [$blockSize, $bytesPerBlock] = self::TYPES[$type] ?? throw new GgufException('Unsupported GGML type: ' . $type);
        if ($elements % $blockSize !== 0) {
            throw new GgufException(
                'Element count ' . $elements . ' is not a multiple of block size ' . $blockSize
                . ' for ' . self::name($type),
            );
        }

        return intdiv($elements, $blockSize) * $bytesPerBlock;
    }

    public static function assertSupported(int $type): void
    {
        if (!isset(self::TYPES[$type])) {
            throw new GgufException('Unsupported GGML type: ' . $type);
        }
    }
}
