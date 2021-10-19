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
use Swow\Socket;
use TypeError;
use function strlen;
use function strpos;
use const SEEK_CUR;

class EofStream extends Socket
{
    /** @var string */
    protected $eof = "\r\n";

    /** @var bool */
    protected $trim = true;

    /** @var Buffer */
    protected $internalBuffer;

    public function __construct(string $eof = "\r\n", int $type = EofStream::TYPE_TCP, bool $trim = true)
    {
        if (!($type & Socket::TYPE_FLAG_STREAM)) {
            throw new InvalidArgumentException('Socket type should be a kind of streams');
        }
        parent::__construct($type);
        $this->__selfConstruct($eof);
    }

    protected function __selfConstruct(string $eof = "\r\n", bool $trim = true)
    {
        $this->eof = $eof;
        $this->trim = $trim;
        $this->internalBuffer = new Buffer();
    }

    public function getEof(): string
    {
        return $this->eof;
    }

    public function accept(?Socket $client = null, ?int $timeout = null)
    {
        if ($client !== null && !($client instanceof self)) {
            throw new TypeError('Client should be an instance of ' . self::class);
        }
        $stream = parent::accept($client, $timeout);
        $stream->__selfConstruct($this->eof, $this->trim);

        return $stream;
    }

    /**
     * @return int package length
     */
    public function recvPacket(Buffer $buffer, ?int $timeout = null)
    {
        $bufferPreviousOffset = $buffer->tell();
        $internalBuffer = $this->internalBuffer;
        $eof = $this->eof;
        $eofOffset = 0;
        $length = 0;
        $expectMore = $internalBuffer->isEmpty();
        while (true) {
            if ($expectMore) {
                try {
                    $buffer->lock();
                    $nread = $this->recvData($internalBuffer, null, $timeout);
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
                    $internalBuffer->truncateFrom($eofOffset);
                    $eofOffset = 0;
                } else {
                    $internalBuffer->extend();
                }
            }
            $expectMore = true;
        }
        $increment = $pos + ($this->trim ? 0 : strlen($eof));
        $buffer
            ->write($internalBuffer->toString(), 0, $increment)
            ->seek($bufferPreviousOffset);
        $length += $increment;
        /* next packet data maybe received */
        $internalBuffer->truncateFrom($pos + strlen($eof));

        return $length;
    }

    public function recvPacketString(?int $timeout = null): string
    {
        $buffer = Buffer::for();
        $this->recvPacket($buffer, $timeout);

        return $buffer->toString();
    }

    /**
     * It's faster, but it may consume more memory when package is small.
     * Use it when expect a big package.
     * @return int package length
     */
    public function recvPacketFast(Buffer $buffer, ?int $timeout = null): int
    {
        $bufferPreviousOffset = $buffer->tell();
        $internalBuffer = $this->internalBuffer;
        $eof = $this->eof;
        $eofOffset = 0;
        while (true) {
            if ($internalBuffer->isEmpty()) {
                $nread = $this->recvData($buffer, null, $timeout);
                $buffer->seek($nread, SEEK_CUR);
            } else {
                $buffer->write($internalBuffer->toString());
                $internalBuffer->clear();
            }
            $pos = strpos($buffer->toString(), $eof, $eofOffset);
            if ($pos !== false) {
                if ($this->trim) {
                    $buffer->truncate($pos);
                }
                break;
            }
            $eofOffset = ($buffer->getLength() - (strlen($eof) - 1));
            if ($eofOffset < 0) {
                $eofOffset = 0;
            }
            if ($buffer->isFull()) {
                $buffer->extend();
            }
        }
        $length = $buffer->tell() - $bufferPreviousOffset;
        $buffer->seek($bufferPreviousOffset);

        return $length;
    }

    public function recvPacketStringFast(?int $timeout = null): string
    {
        $buffer = new Buffer(Buffer::PAGE_SIZE);
        $this->recvPacketFast($buffer, $timeout);

        return $buffer->toString();
    }

    /** @return $this */
    public function sendPacketString(string $packet)
    {
        return $this->write([$packet, $this->eof]);
    }

    /** @return $this */
    public function writePackets(array $packets, ?int $timeout = null)
    {
        $packets[] = $this->eof;

        return $this->write($packets, $timeout);
    }
}
