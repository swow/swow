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

use InvalidArgumentException;
use RuntimeException;
use Stringable;
use Swow\Buffer;
use TypeError;
use ValueError;

use function get_debug_type;
use function sprintf;
use function Swow\Debug\isStringable;

use const SEEK_CUR;
use const SEEK_END;
use const SEEK_SET;

class BufferStream implements StreamPlusInterface
{
    protected Buffer $buffer;
    protected int $offset = 0;

    /**
     * @param scalar|Buffer $data
     */
    public function __construct(mixed $data)
    {
        if ($data instanceof Buffer) {
            $this->buffer = $data;
            return;
        }
        if (!isStringable($data)) {
            throw new TypeError(sprintf(
                '%s(): Argument#1 ($buffer) must be of type scalar or implement %s, %s given',
                __METHOD__, Stringable::class, get_debug_type($data)
            ));
        }
        $data = (string) $data;
        $buffer = new Buffer(0);
        if ($data !== '') {
            $buffer->append($data);
        }
        $this->buffer = $buffer;
    }

    public function close(): void
    {
        $this->buffer->close();
    }

    /** @return never */
    public function detach(): mixed
    {
        throw new RuntimeException('Can not detach');
    }

    public function getSize(): int
    {
        return $this->buffer->getLength();
    }

    public function tell(): int
    {
        return $this->offset;
    }

    public function eof(): bool
    {
        return $this->offset === $this->buffer->getLength();
    }

    public function isSeekable(): bool
    {
        return true;
    }

    public function seek(mixed $offset, mixed $whence = SEEK_SET): void
    {
        $thisOffset = match ($whence) {
            SEEK_SET => $offset,
            SEEK_CUR => $this->offset + $offset,
            SEEK_END => $this->getSize() + $offset,
            default => throw new ValueError(sprintf('%s(): Argument#2 ($whence) is invalid', __METHOD__)),
        };
        if ($thisOffset < 0 || $thisOffset > $this->getSize()) {
            throw new InvalidArgumentException('Offset is overflow');
        }
    }

    public function rewind(): void
    {
        $this->offset = 0;
    }

    public function isWritable(): bool
    {
        return $this->buffer->isLocked();
    }

    public function write(mixed $string): int
    {
        return $this->buffer->write($this->offset, (string) $string);
    }

    public function isReadable(): bool
    {
        return $this->buffer->isLocked();
    }

    public function read(mixed $length): string
    {
        return $this->buffer->read($this->offset, $length);
    }

    public function getContents(): string
    {
        return $this->buffer->read($this->offset, -1);
    }

    public function getMetadata(mixed $key = null): mixed
    {
        $metadata = $this->buffer->__debugInfo();

        return $key === null ? $metadata : ($metadata[$key] ?? null);
    }

    public function toString(): string
    {
        return (string) $this->buffer;
    }

    public function __toString(): string
    {
        return (string) $this->buffer;
    }
}
