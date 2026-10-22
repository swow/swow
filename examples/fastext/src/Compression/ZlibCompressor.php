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

use function strlen;

use const ZLIB_ENCODING_GZIP;
use const ZLIB_FINISH;
use const ZLIB_NO_FLUSH;
use const ZLIB_SYNC_FLUSH;

final class ZlibCompressor implements Compressor
{
    private const CHUNK_SIZE = 1024 * 1024;

    public function name(): string
    {
        return 'zlib';
    }

    public function attr(): int
    {
        return Metadata::ATTR_COMPRESS_ZLIB;
    }

    public function compressFile(string $source, string $target): void
    {
        $in = fopen($source, 'rb');
        $out = fopen($target, 'wb');
        if ($in === false || $out === false) {
            throw new RuntimeException('Unable to open zlib streams');
        }

        $context = deflate_init(ZLIB_ENCODING_GZIP, ['level' => 3]);
        if ($context === false) {
            throw new RuntimeException('Unable to initialize deflate');
        }

        try {
            while (!feof($in)) {
                $chunk = fread($in, self::CHUNK_SIZE);
                if ($chunk === false) {
                    throw new RuntimeException("Unable to read: {$source}");
                }
                if ($chunk === '') {
                    break;
                }
                $data = deflate_add($context, $chunk, ZLIB_NO_FLUSH);
                if ($data === false || fwrite($out, $data) !== strlen($data)) {
                    throw new RuntimeException("Unable to write: {$target}");
                }
            }
            $data = deflate_add($context, '', ZLIB_FINISH);
            if ($data === false || fwrite($out, $data) !== strlen($data)) {
                throw new RuntimeException("Unable to finish: {$target}");
            }
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
            throw new RuntimeException('Unable to open zlib streams');
        }

        $context = inflate_init(ZLIB_ENCODING_GZIP);
        if ($context === false) {
            throw new RuntimeException('Unable to initialize inflate');
        }

        try {
            while (!feof($in)) {
                $chunk = fread($in, self::CHUNK_SIZE);
                if ($chunk === false) {
                    throw new RuntimeException("Unable to read: {$source}");
                }
                if ($chunk === '') {
                    break;
                }
                $data = inflate_add($context, $chunk, ZLIB_SYNC_FLUSH);
                if ($data === false || fwrite($out, $data) !== strlen($data)) {
                    throw new RuntimeException("Unable to write: {$target}");
                }
            }
            $data = inflate_add($context, '', ZLIB_FINISH);
            if ($data === false || fwrite($out, $data) !== strlen($data)) {
                throw new RuntimeException("Unable to finish: {$target}");
            }
        } finally {
            fclose($in);
            fclose($out);
        }
    }
}
