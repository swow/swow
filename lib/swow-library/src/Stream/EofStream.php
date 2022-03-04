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
use Swow\Socket;
use function strlen;
use function strpos;
use const SEEK_CUR;

class EofStream extends Socket
{
    protected string $eof = "\r\n";

    protected Buffer $internalBuffer;

    use MaxMessageLengthTrait;

    public function __construct(string $eof = "\r\n", int $type = self::TYPE_TCP)
    {
        if (!($type & static::TYPE_FLAG_STREAM)) {
            throw new InvalidArgumentException('Socket type should be a kind of streams');
        }
        parent::__construct($type);
        $this->__selfConstruct($eof);
    }

    protected function __selfConstruct(string $eof = "\r\n"): void
    {
        $this->eof = $eof;
        $this->internalBuffer = new Buffer();
    }

    public function getEof(): string
    {
        return $this->eof;
    }

    public function accept(?int $timeout = null): static
    {
        /** @var static $connection */
        $connection = parent::accept($timeout);
        $connection->__selfConstruct($this->eof);
        $connection->maxMessageLength = $this->maxMessageLength;

        return $connection;
    }

    public function acceptTo(Socket $connection, ?int $timeout = null): static
    {
        $ret = parent::acceptTo($connection, $timeout);
        if ($connection instanceof self) {
            $connection->eof = $this->eof;
            $connection->maxMessageLength = $this->maxMessageLength;
        }

        return $ret;
    }

    /**
     * @return int message length
     */
    public function recvMessage(Buffer $buffer, ?int $timeout = null)
    {
        $bufferPreviousOffset = $buffer->tell();
        $internalBuffer = $this->internalBuffer;
        $eof = $this->eof;
        $eofOffset = 0;
        $maxMessageLength = $this->maxMessageLength;
        $length = 0;
        $expectMore = $internalBuffer->isEmpty();
        while (true) {
            if ($expectMore) {
                try {
                    $buffer->lock();
                    $nread = $this->recvData($internalBuffer, -1, $timeout);
                } finally {
                    $buffer->unlock();
                }
                $internalBuffer->seek($nread, SEEK_CUR);
            }
            $pos = strpos($internalBuffer->toString(), $eof, $eofOffset);
            if ($pos !== false) {
                break;
            }
            $eofOffset = ($internalBuffer->getLength() - (strlen($eof) - 1));
            if ($eofOffset < 0) {
                $eofOffset = 0;
            }
            if ($internalBuffer->isFull()) {
                if ($eofOffset > 0) {
                    $buffer->write($internalBuffer->toString(), 0, $eofOffset);
                    $length += $eofOffset;
                    if ($length > $maxMessageLength) {
                        throw new MessageTooLargeException($length, $maxMessageLength);
                    }
                    $internalBuffer->truncateFrom($eofOffset);
                    $eofOffset = 0;
                } else {
                    $internalBuffer->extend();
                }
            }
            $expectMore = true;
        }
        $length += $pos;
        if ($length > $maxMessageLength) {
            throw new MessageTooLargeException($length, $maxMessageLength);
        }
        $buffer
            ->write($internalBuffer->toString(), 0, $pos)
            ->seek($bufferPreviousOffset);
        /* next packet data maybe received */
        $internalBuffer->truncateFrom($pos + strlen($eof));

        return $length;
    }

    public function recvMessageString(?int $timeout = null): string
    {
        $buffer = Buffer::for();
        $this->recvMessage($buffer, $timeout);

        return $buffer->toString();
    }

    /**
     * It's faster, but it may consume more memory when message is small.
     * Use it when expect a big package.
     * @return int message length
     */
    public function recvMessageFast(Buffer $buffer, ?int $timeout = null): int
    {
        $bufferPreviousOffset = $buffer->tell();
        $internalBuffer = $this->internalBuffer;
        $eof = $this->eof;
        $eofOffset = 0;
        $maxMessageLength = $this->maxMessageLength;
        while (true) {
            if ($internalBuffer->isEmpty()) {
                $nread = $this->recvData($buffer, -1, $timeout);
                $buffer->seek($nread, SEEK_CUR);
            } else {
                $buffer->write($internalBuffer->toString());
                $internalBuffer->clear();
            }
            $pos = strpos($buffer->toString(), $eof, $eofOffset);
            if ($pos !== false) {
                $buffer->truncate($pos);
                break;
            }
            $eofOffset = ($buffer->getLength() - (strlen($eof) - 1));
            if ($eofOffset < 0) {
                $eofOffset = 0;
            }
            if ($eofOffset > $maxMessageLength) {
                throw new MessageTooLargeException($eofOffset, $maxMessageLength);
            }
            if ($buffer->isFull()) {
                $buffer->extend();
            }
        }
        $length = $buffer->tell() - $bufferPreviousOffset;
        $buffer->seek($bufferPreviousOffset);

        return $length;
    }

    public function recvMessageStringFast(?int $timeout = null): string
    {
        $buffer = new Buffer(Buffer::PAGE_SIZE);
        $this->recvMessageFast($buffer, $timeout);

        return $buffer->toString();
    }

    public function sendMessageString(string $message, ?int $timeout = null): static
    {
        return $this->write([$message, $this->eof], $timeout);
    }

    /** @param array<mixed> $messages */
    public function writeMessages(array $messages, ?int $timeout = null): static
    {
        $messages[] = $this->eof;

        return $this->write($messages, $timeout);
    }
}
