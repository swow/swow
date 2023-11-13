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
use Swow\Socket;

use function is_array;
use function strlen;
use function strpos;

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
        $this->eof = $eof;
        $this->internalBuffer = new Buffer(Buffer::COMMON_SIZE);
    }

    public function getEof(): string
    {
        return $this->eof;
    }

    public function setEof(string $eof): static
    {
        $this->eof = $eof;
        return $this;
    }

    public function accept(?int $timeout = null): static
    {
        $connection = parent::accept($timeout);
        $connection->eof = $this->eof;
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
        $internalBuffer = $this->internalBuffer;
        $eof = $this->eof;
        $eofOffset = 0;
        $maxMessageLength = $this->maxMessageLength;
        $nWrite = 0;
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
                    $nWrite += $buffer->write($offset + $nWrite, $internalBuffer, length: $eofOffset);
                    if ($nWrite > $maxMessageLength) {
                        throw new MessageTooLargeException($nWrite, $maxMessageLength);
                    }
                    $internalBuffer->truncateFrom($eofOffset);
                    $eofOffset = 0;
                } else {
                    $internalBuffer->extend();
                }
            }
        }
        if ($nWrite + $pos > $maxMessageLength) {
            throw new MessageTooLargeException($nWrite, $maxMessageLength);
        }
        $nWrite += $buffer->write($offset + $nWrite, $internalBuffer, length: $pos);
        /* next packet data maybe received */
        $internalBuffer->truncateFrom($pos + strlen($eof));

        return $nWrite;
    }

    public function recvMessageString(?int $timeout = null): string
    {
        $buffer = new Buffer(0);
        $this->recvMessage($buffer, $timeout);

        return $buffer->toString();
    }

    /**
     * It's faster, but it may consume more memory when message is small.
     * Use it when expect a big package.
     * @param ?int $offset default value is $buffer->getLength()
     * @return int message length
     */
    public function recvMessageFast(Buffer $buffer, ?int $offset = null, ?int $timeout = null): int
    {
        $offset ??= $buffer->getLength();
        $internalBuffer = $this->internalBuffer;
        $eof = $this->eof;
        $eofOffset = $offset;
        $maxMessageLength = $this->maxMessageLength;
        while (true) {
            if ($internalBuffer->isEmpty()) {
                $this->recvData($buffer, timeout: $timeout);
            } else {
                $buffer->write($offset, $internalBuffer);
                $internalBuffer->clear();
            }
            $pos = strpos($buffer->toString(), $eof, $eofOffset);
            if ($pos !== false) {
                $length = $pos - $offset;
                if ($length > $maxMessageLength) {
                    throw new MessageTooLargeException($eofOffset, $maxMessageLength);
                }
                $internalBuffer->append($buffer, $pos + strlen($eof));
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
        return $length;
    }

    public function recvMessageStringFast(?int $timeout = null): string
    {
        $buffer = new Buffer(Buffer::PAGE_SIZE);
        $this->recvMessageFast($buffer, $timeout);

        return $buffer->toString();
    }

    public function sendMessage(string|Stringable $message, int $start = 0, int $length = -1, ?int $timeout = null): static
    {
        return $this->write([[$message, $start, $length], $this->eof], $timeout);
    }

    /** @param non-empty-array<string|Stringable|Buffer|array{0: string|Stringable|Buffer, 1?: int, 2?: int}|null> $chunks */
    public function sendMessageChunks(array $chunks, ?int $timeout = null): static
    {
        $chunks[] = $this->eof;

        return $this->write($chunks, $timeout);
    }

    /** @param non-empty-array<string|Stringable|Buffer|array{0: string|Stringable|Buffer, 1?: int, 2?: int}|array<non-empty-array<string|Stringable|Buffer|array{0: string|Stringable|Buffer, 1?: int, 2?: int}|null>>|null> $messages */
    public function sendMessages(array $messages, ?int $timeout = null): static
    {
        $eof = $this->eof;
        $_messages = [];
        foreach ($messages as $message) {
            if (is_array($message)) {
                foreach ($message as $chunk) {
                    $_messages[] = $chunk;
                }
            } else {
                $_messages[] = $message;
            }
            $_messages[] = $eof;
        }
        /** @var non-empty-array<string|Stringable|Buffer|array{0: string|Stringable|Buffer, 1?: int, 2?: int}|null> $_messages */
        return $this->write($_messages, $timeout);
    }
}
