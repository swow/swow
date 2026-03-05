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

use Swow\Http\Protocol\ChunkedBodyStream;
use Throwable;

use const SEEK_SET;

final class ChunkedBodyPsrStream implements StreamPlusInterface
{
    /** 仅做协议层到 PSR7 的适配，所有真实状态都在底层 chunked stream 中 */
    public function __construct(
        protected ChunkedBodyStream $chunkedBodyStream,
    ) {
    }

    /** 暴露底层对象供高级场景使用（如注册完成回调）。 */
    public function getChunkedBodyStream(): ChunkedBodyStream
    {
        return $this->chunkedBodyStream;
    }

    public function __toString(): string
    {
        try {
            return $this->toString();
        } catch (Throwable) {
            return '';
        }
    }

    public function getSize(): ?int
    {
        return $this->chunkedBodyStream->getSize();
    }

    public function tell(): int
    {
        return $this->chunkedBodyStream->tell();
    }

    public function eof(): bool
    {
        return $this->chunkedBodyStream->eof();
    }

    public function isSeekable(): bool
    {
        return $this->chunkedBodyStream->isSeekable();
    }

    public function seek(mixed $offset, mixed $whence = SEEK_SET): void
    {
        $this->chunkedBodyStream->seek($offset, $whence);
    }

    public function rewind(): void
    {
        $this->chunkedBodyStream->rewind();
    }

    public function isWritable(): bool
    {
        return $this->chunkedBodyStream->isWritable();
    }

    public function write(mixed $string): int
    {
        return $this->chunkedBodyStream->write($string);
    }

    public function isReadable(): bool
    {
        return $this->chunkedBodyStream->isReadable();
    }

    public function read(mixed $length): string
    {
        return $this->chunkedBodyStream->read($length);
    }

    public function getContents(): string
    {
        return $this->chunkedBodyStream->getContents();
    }

    public function getMetadata(mixed $key = null): mixed
    {
        return $this->chunkedBodyStream->getMetadata($key);
    }

    public function detach(): mixed
    {
        return $this->chunkedBodyStream->detach();
    }

    public function close(): void
    {
        $this->chunkedBodyStream->close();
    }

    public function toString(): string
    {
        return $this->chunkedBodyStream->toString();
    }
}
