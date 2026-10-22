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

use Swow\Examples\Fastext\Compressor;

final class NoneCompressor implements Compressor
{
    public function name(): string
    {
        return 'none';
    }

    public function attr(): int
    {
        return 0;
    }

    public function compressFile(string $source, string $target): void
    {
        if (!copy($source, $target)) {
            throw new \RuntimeException("Unable to copy: {$source}");
        }
    }

    public function decompressFile(string $source, string $target): void
    {
        if (!copy($source, $target)) {
            throw new \RuntimeException("Unable to copy: {$source}");
        }
    }
}
