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
use Swow\Pack\Format;
use Swow\Socket;
use function pack;
use function strlen;
use function unpack;

class LengthStream extends Socket
{
    protected string $format = Format::UINT32_BE;

    protected int $formatSize = 4;

    use MaxMessageLengthTrait;

    public function __construct(string $format = Format::UINT32_BE, int $type = self::TYPE_TCP)
    {
        if (!($type & static::TYPE_FLAG_STREAM)) {
            throw new InvalidArgumentException('Socket type should be a kind of streams');
        }
        parent::__construct($type);
        $this->setFormat($format);
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
        /** @var static $connection */
        $connection = parent::accept($timeout);
        $connection->setFormat($this->format);
        $connection->maxMessageLength = $this->maxMessageLength;

        return $connection;
    }

    public function acceptTo(Socket $connection, ?int $timeout = null): static
    {
        $ret = parent::acceptTo($connection, $timeout);
        if ($connection instanceof self) {
            $connection->format = $this->format;
            $connection->formatSize = $this->formatSize;
            $connection->maxMessageLength = $this->maxMessageLength;
        }

        return $ret;
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

    public function sendMessageString(string $string, ?int $timeout = null, int $offset = 0, int $length = -1): static
    {
        return $this->write([pack($this->format, strlen($string)), [$string, $offset, $length]], $timeout);
    }
}
