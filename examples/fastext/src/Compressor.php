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

namespace Swow\Examples\Fastext;

interface Compressor
{
    public function name(): string;

    public function attr(): int;

    public function compressFile(string $source, string $target): void;

    public function decompressFile(string $source, string $target): void;
}
