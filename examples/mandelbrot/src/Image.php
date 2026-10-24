<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

use InvalidArgumentException;
use RuntimeException;

use function strlen;

/**
 * Minimal PNG writer for BGRA pixel data.
 *
 * Uses only zlib, so PNG output works with FFI disabled.
 */
final class Image
{
    private const SIGNATURE = "\x89PNG\r\n\x1a\n";
    private const COMPRESSION_LEVEL = 6;
    private const COLOR_TYPE_RGBA = 6;
    private const BIT_DEPTH = 8;

    public static function encode(string $bgra, int $width, int $height): string
    {
        if ($width <= 0 || $height <= 0) {
            throw new InvalidArgumentException('width and height must be positive');
        }

        $rowBytes = $width * 4;
        if (strlen($bgra) !== $rowBytes * $height) {
            throw new InvalidArgumentException('BGRA data does not match the image size');
        }

        $raw = '';
        for ($y = 0; $y < $height; $y++) {
            $raw .= "\x00";
            $base = $y * $rowBytes;
            for ($i = 0; $i < $rowBytes; $i += 4) {
                $offset = $base + $i;
                $raw .= $bgra[$offset + 2] . $bgra[$offset + 1] . $bgra[$offset] . $bgra[$offset + 3];
            }
        }

        $compressed = gzcompress($raw, self::COMPRESSION_LEVEL);
        if ($compressed === false) {
            throw new RuntimeException('zlib failed to compress PNG data');
        }

        $ihdr = pack(
            'NNCCCCC',
            $width,
            $height,
            self::BIT_DEPTH,
            self::COLOR_TYPE_RGBA,
            0,
            0,
            0,
        );

        return self::SIGNATURE
            . self::chunk('IHDR', $ihdr)
            . self::chunk('IDAT', $compressed)
            . self::chunk('IEND', '');
    }

    public static function save(string $path, string $bgra, int $width, int $height): void
    {
        $png = self::encode($bgra, $width, $height);
        if (file_put_contents($path, $png) !== strlen($png)) {
            throw new RuntimeException("cannot write PNG file: {$path}");
        }
    }

    private static function chunk(string $type, string $data): string
    {
        return pack('N', strlen($data))
            . $type
            . $data
            . pack('N', crc32($type . $data));
    }
}
