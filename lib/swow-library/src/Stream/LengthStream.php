<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Stream;

use InvalidArgumentException;
use Swow\Pack\Format;
use Swow\Socket;
use TypeError;
use function pack;
use function strlen;
use function unpack;

class LengthStream extends Socket
{
    protected string $format = Format::UINT32_BE;

    protected int  $formatSize = 4;

    use MaxMessageLengthTrait;

    public function __construct(string $format = Format::UINT32_BE, int $type = self::TYPE_TCP)
    {
        if (!($type & Socket::TYPE_FLAG_STREAM)) {
            throw new InvalidArgumentException('Socket type should be a kind of streams');
        }
        parent::__construct($type);
    }

    public function getFormat(): string
    {
        return $this->format;
    }

    /** @return $this */
    public function setFormat(string $format)
    {
        $this->format = $format;
        $this->formatSize = Format::getSize($format);

        return $this;
    }

    public function getFormatSize(): int
    {
        return $this->formatSize;
    }

    /** @return $this */
    public function accept(?Socket $client = null, ?int $timeout = null)
    {
        if ($client !== null && !($client instanceof self)) {
            throw new TypeError('Client should be an instance of ' . self::class);
        }
        $stream = parent::accept($client, $timeout);
        $stream->format = $this->format;
        $stream->formatSize = $this->formatSize;
        $stream->maxMessageLength = $this->maxMessageLength;

        return $stream;
    }

    /** @return int message length */
    public function recvMessage(Buffer $buffer, ?int $timeout = null): int
    {
        $head = $this->readString($this->formatSize, $timeout);
        $length = unpack($this->format, $head)[1];
        if ($length > $this->maxMessageLength) {
            throw new MessageTooLargeException($length, $this->maxMessageLength);
        }
        $writableSize = $buffer->getWritableSize();
        if ($writableSize < $length) {
            $buffer->realloc($length - $writableSize);
        }
        $this->read($buffer, $length, $timeout);

        return $length;
    }

    public function recvMessageString(?int $timeout = null): string
    {
        $buffer = Buffer::for();
        $this->recvMessage($buffer, $timeout);

        return $buffer->toString();
    }

    /** @return $this */
    public function sendMessageString(string $string, ?int $timeout = null, int $offset = 0, int $length = -1)
    {
        return $this->write([pack($this->format, strlen($string)), [$string, $offset, $length]], $timeout);
    }
}
