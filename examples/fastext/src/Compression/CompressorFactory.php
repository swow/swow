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

namespace Swow\Examples\Fastext\Compression;

use RuntimeException;
use Swow\Examples\Fastext\Compressor;

final class CompressorFactory
{
    public static function create(string $choice): Compressor
    {
        return match ($choice) {
            'auto' => ZstdCompressor::isAvailable() ? new ZstdCompressor() : new ZlibCompressor(),
            'zstd' => ZstdCompressor::isAvailable()
                ? new ZstdCompressor()
                : throw new RuntimeException('The zstd stream filter is not available'),
            'zlib' => new ZlibCompressor(),
            'none' => new NoneCompressor(),
            default => throw new RuntimeException("Unknown compression: {$choice}"),
        };
    }
}
