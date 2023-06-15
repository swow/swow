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

namespace Swow\Psr7\Message;

use Psr\Http\Message\StreamInterface;

use const SEEK_SET;

interface StreamPlusInterface extends StreamInterface
{
    public function getSize(): ?int;

    public function tell(): int;

    public function eof(): bool;

    public function isSeekable(): bool;

    /**
     * @param int $offset
     * @param int $whence
     */
    public function seek(mixed $offset, mixed $whence = SEEK_SET): void;

    public function rewind(): void;

    public function isWritable(): bool;

    public function write(mixed $string): int;

    public function isReadable(): bool;

    /** @param int $length */
    public function read(mixed $length): string;

    public function getContents(): string;

    public function getMetadata(mixed $key = null): mixed;

    /** @return resource|null */
    public function detach(): mixed;

    public function close(): void;

    public function toString(): string;
}
