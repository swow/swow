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

namespace Swow\Stream;

use InvalidArgumentException;
use Stringable;
use Swow\Buffer;
use Swow\Pack\Format;
use Swow\Socket;

use function assert;
use function is_array;
use function pack;
use function strlen;
use function unpack;

class LengthStream extends Socket
{
    protected string $format = Format::UINT32_BE;

    protected int $formatSize = 4;

    protected Buffer $internalBuffer;

    use MaxMessageLengthTrait;

    public function __construct(string $format = Format::UINT32_BE, int $type = self::TYPE_TCP)
    {
        if (!($type & static::TYPE_FLAG_STREAM)) {
            throw new InvalidArgumentException('Socket type should be a kind of streams');
        }
        parent::__construct($type);
        $this->setFormat($format);
        $this->internalBuffer = new Buffer(Buffer::COMMON_SIZE);
    }

    public function getFormat(): string
    {
        return $this->format;
    }

    public function setFormat(string $format): static
    {
        $this->format = $format;
        $this->formatSize = Format::getSize($format);

        return $this;
    }

    public function getFormatSize(): int
    {
        return $this->formatSize;
    }

    public function accept(?int $timeout = null): static
    {
        $connection = parent::accept($timeout);
        $connection->format = $this->format;
        $connection->formatSize = $this->formatSize;
        $connection->maxMessageLength = $this->maxMessageLength;
        $connection->internalBuffer = new Buffer(Buffer::COMMON_SIZE);

        return $connection;
    }

    /**
     * @param ?int $offset default value is $buffer->getLength()
     * @return int message length
     */
    public function recvMessage(Buffer $buffer, ?int $offset = null, ?int $timeout = null): int
    {
        $offset ??= $buffer->getLength();
        $format = $this->getFormat();
        $formatSize = $this->formatSize;
        $maxMessageLength = $this->maxMessageLength;
        $internalBuffer = $this->internalBuffer;
        $expectMore = $internalBuffer->isEmpty();
        while (true) {
            if ($expectMore) {
                try {
                    $buffer->lock();
                    $this->recvData($internalBuffer, $internalBuffer->getLength(), -1, $timeout);
                } finally {
                    $buffer->unlock();
                }
            } else {
                $expectMore = true;
            }
            $internalLength = $internalBuffer->getLength();
            if ($internalLength >= $formatSize) {
                break;
            }
        }
        $length = unpack($format, $internalBuffer->toString())[1];
        if ($length > $maxMessageLength) {
            throw new MessageTooLargeException($length, $maxMessageLength);
        }
        $needSize = $offset + $length;
        $bufferSize = $buffer->getSize();
        if ($needSize > $bufferSize) {
            $buffer->realloc($needSize);
        }
        $nWrite = $buffer->write($offset, $internalBuffer, $formatSize);
        $internalBuffer->truncateFrom($formatSize + $nWrite);
        if ($nWrite < $length) {
            $nWrite += $this->read($buffer, $offset + $nWrite, $length - $nWrite, $timeout);
        }
        assert($nWrite === $length);

        return $length;
    }

    public function recvMessageString(?int $timeout = null): string
    {
        $buffer = new Buffer(0);
        $this->recvMessage($buffer, $timeout);

        return $buffer->toString();
    }

    public function sendMessage(string|Stringable $string, int $start = 0, int $length = -1, ?int $timeout = null): static
    {
        return $this->write([pack($this->format, strlen((string) $string)), [$string, $start, $length]], $timeout);
    }

    /** @param non-empty-array<string|Stringable|Buffer|array{0: string|Stringable|Buffer, 1?: int, 2?: int}|null> $chunks */
    public function sendMessageChunks(array $chunks, ?int $timeout = null): static
    {
        $length = 0;
        // TODO: make Socket support to calculate the length of chunks...
        foreach ($chunks as $chunk) {
            if (is_array($chunk)) {
                if (isset($chunk[2]) && ($chunkLength = (int) $chunk[2]) > 0) {
                    $length += $chunkLength;
                } elseif (isset($chunk[1])) {
                    $length += strlen((string) $chunk[0]) - ((int) $chunk[1]);
                } else {
                    $length += strlen((string) $chunk[0]);
                }
            } else {
                $length += strlen((string) $chunk);
            }
        }

        return $this->write([pack($this->format, $length), ...$chunks], $timeout);
    }
}
