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
use Swow\Examples\Fastext\Metadata;

use function in_array;

use const STREAM_FILTER_READ;
use const STREAM_FILTER_WRITE;

final class ZstdCompressor implements Compressor
{
    private const COMPRESS_FILTER = 'zstd.compress';
    private const DECOMPRESS_FILTER = 'zstd.uncompress';

    public static function isAvailable(): bool
    {
        $filters = stream_get_filters();

        return in_array(self::COMPRESS_FILTER, $filters, true) &&
            in_array(self::DECOMPRESS_FILTER, $filters, true);
    }

    public function name(): string
    {
        return 'zstd';
    }

    public function attr(): int
    {
        return Metadata::ATTR_COMPRESS_ZSTD;
    }

    public function compressFile(string $source, string $target): void
    {
        $in = fopen($source, 'rb');
        $out = fopen($target, 'wb');
        if ($in === false || $out === false) {
            throw new RuntimeException('Unable to open zstd streams');
        }

        $filter = stream_filter_append($out, self::COMPRESS_FILTER, STREAM_FILTER_WRITE);
        if ($filter === false) {
            throw new RuntimeException('Unable to create zstd compress filter');
        }

        try {
            if (stream_copy_to_stream($in, $out) === false) {
                throw new RuntimeException("Unable to compress: {$source}");
            }
            stream_filter_remove($filter);
        } finally {
            fclose($in);
            fclose($out);
        }
    }

    public function decompressFile(string $source, string $target): void
    {
        $in = fopen($source, 'rb');
        $out = fopen($target, 'wb');
        if ($in === false || $out === false) {
            throw new RuntimeException('Unable to open zstd streams');
        }

        $filter = stream_filter_append($in, self::DECOMPRESS_FILTER, STREAM_FILTER_READ);
        if ($filter === false) {
            throw new RuntimeException('Unable to create zstd decompress filter');
        }

        try {
            if (stream_copy_to_stream($in, $out) === false) {
                throw new RuntimeException("Unable to decompress: {$source}");
            }
            stream_filter_remove($filter);
        } finally {
            fclose($in);
            fclose($out);
        }
    }
}
