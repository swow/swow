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

namespace Swow\Http\Protocol;

final class H2HeaderBlock
{
    private string $buffer = '';

    private bool $completed = false;

    public function __construct(
        private int $streamId,
        private int $flags = 0,
    ) {}

    public function getStreamId(): int
    {
        return $this->streamId;
    }

    public function isCompleted(): bool
    {
        return $this->completed;
    }

    public function getFlags(): int
    {
        return $this->flags;
    }

    public function append(H2Frame $frame): void
    {
        if ($this->completed) {
            throw H2Exception::forStreamError($this->streamId, 'Header block already completed');
        }

        if ($frame->streamId !== $this->streamId) {
            throw H2Exception::forConnectionError('CONTINUATION frame stream does not match active header block');
        }

        if ($frame->type !== H2Frame::TYPE_HEADERS && $frame->type !== H2Frame::TYPE_CONTINUATION) {
            throw H2Exception::forConnectionError('Header block was interrupted by a non-continuation frame');
        }

        $this->buffer .= $frame->payload;
        $this->flags = $frame->flags;
        if (($frame->flags & H2Frame::FLAG_END_HEADERS) !== 0) {
            $this->completed = true;
        }
    }

    public function getBuffer(): string
    {
        return $this->buffer;
    }
}
