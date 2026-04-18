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

use Closure;
use InvalidArgumentException;
use RuntimeException;
use Throwable;
use ValueError;

use function min;
use function strlen;
use function strtolower;

use const SEEK_CUR;
use const SEEK_END;
use const SEEK_SET;

final class ChunkedBodyStream
{
    /* 这是协议层流式 body 对象，核心语义：
     * 1) 按需拉取：read/seek 只推进到需要的长度；
     * 2) 可重读：已拉取内容都会缓存到 bodyBuffer；
     * 3) 全量入口（toString/getSize/getContents）会推进到完成态；
     * 4) 完成后触发一次 onCompleted 回调，用于上层做 header 归一化。 */

    /** 当前读取游标；缓冲区始终保存“从 0 开始的完整已读数据” */
    protected int $offset = 0;

    protected bool $closed = false;

    /** 只通知一次，避免 header 归一化等收尾逻辑重复执行 */
    protected bool $completionNotified = false;

    /** @var array<Closure> */
    protected array $completionCallbacks = [];

    public function __construct(
        protected ChunkedBodyState $state,
        protected Closure $fillToCallback,
        protected Closure $fillStreamingCallback,
        protected Closure $fillAllCallback,
        protected Closure $closeCallback,
    ) {
    }

    /** 防止 close 后继续读取，避免状态机被重复驱动。 */
    protected function ensureOpen(): void
    {
        if ($this->closed) {
            throw new RuntimeException('Stream is closed');
        }
    }

    protected function fillTo(int $targetLength): void
    {
        if ($this->state->finalized || $this->state->bodyBuffer->getLength() >= $targetLength) {
            $this->notifyCompletionIfNeeded();
            return;
        }
        ($this->fillToCallback)($targetLength);
        $this->notifyCompletionIfNeeded();
    }

    /**
     * 流式推进：做一轮 IO 后即返回，不保证 bodyBuffer 达到 targetLength。
     * targetLength 仅作为读取量上限，实际返回量取决于单次 IO 收到的数据。
     */
    protected function fillStreaming(int $targetLength): void
    {
        if ($this->state->finalized || $this->state->bodyBuffer->getLength() >= $targetLength) {
            $this->notifyCompletionIfNeeded();
            return;
        }
        ($this->fillStreamingCallback)($targetLength);
        $this->notifyCompletionIfNeeded();
    }

    protected function fillAll(): void
    {
        // toString/getSize/getContents 等“全量语义”入口会走这里，直到 MESSAGE_COMPLETE。
        if ($this->state->finalized) {
            $this->notifyCompletionIfNeeded();
            return;
        }
        ($this->fillAllCallback)();
        $this->notifyCompletionIfNeeded();
    }

    /** 统一完成事件出口：只触发一次，避免重复回调造成重复收尾。 */
    protected function notifyCompletionIfNeeded(): void
    {
        if ($this->completionNotified || !$this->state->finalized) {
            return;
        }
        $this->completionNotified = true;
        foreach ($this->completionCallbacks as $callback) {
            $callback();
        }
    }

    public function onCompleted(Closure $callback): void
    {
        // 已完成时立即回调，避免调用方还要额外判断完成态。
        if ($this->completionNotified || $this->state->finalized) {
            $callback();
            return;
        }
        $this->completionCallbacks[] = $callback;
    }

    public function __toString(): string
    {
        try {
            return $this->toString();
        } catch (Throwable) {
            return '';
        }
    }

    public function close(): void
    {
        if ($this->closed) {
            return;
        }
        $this->closed = true;
        ($this->closeCallback)();
        // close 可能触发“内部短时清尾”，这里再兜底触发完成回调。
        $this->notifyCompletionIfNeeded();
    }

    public function detach(): mixed
    {
        $this->close();
        return null;
    }

    public function getSize(): ?int
    {
        $this->ensureOpen();
        $this->fillAll();
        return $this->state->bodyBuffer->getLength();
    }

    public function tell(): int
    {
        $this->ensureOpen();
        return $this->offset;
    }

    public function eof(): bool
    {
        return $this->state->complete && $this->offset >= $this->state->bodyBuffer->getLength();
    }

    public function isSeekable(): bool
    {
        return true;
    }

    public function seek(mixed $offset, mixed $whence = SEEK_SET): void
    {
        $this->ensureOpen();
        $offset = (int) $offset;
        $whence = (int) $whence;
        $targetOffset = match ($whence) {
            SEEK_SET => $offset,
            SEEK_CUR => $this->offset + $offset,
            SEEK_END => $this->state->bodyBuffer->getLength() + $offset,
            default => throw new ValueError('Invalid whence'),
        };
        if ($targetOffset < 0) {
            throw new InvalidArgumentException('Offset is overflow');
        }
        $this->fillTo($targetOffset);
        // 未完成前不允许 seek 到尚未缓冲的位置，避免表现出“可随机访问网络流”的假象。
        if (!$this->state->complete && $targetOffset > $this->state->bodyBuffer->getLength()) {
            throw new RuntimeException('Unable to seek beyond buffered content before stream completion');
        }
        if ($targetOffset > $this->state->bodyBuffer->getLength()) {
            throw new InvalidArgumentException('Offset is overflow');
        }
        $this->offset = $targetOffset;
    }

    public function rewind(): void
    {
        $this->seek(0);
    }

    public function isWritable(): bool
    {
        return false;
    }

    public function write(mixed $string): int
    {
        throw new RuntimeException('Cannot write to a read-only stream');
    }

    public function isReadable(): bool
    {
        return !$this->closed;
    }

    public function read(mixed $length): string
    {
        $this->ensureOpen();
        $length = (int) $length;
        if ($length <= 0) {
            return '';
        }
        $targetOffset = $this->offset + $length;
        // 流式推进：一轮 IO 后即返回，下方 min($length, $available) 保证不超读
        $this->fillStreaming($targetOffset);
        $available = $this->state->bodyBuffer->getLength() - $this->offset;
        if ($available <= 0) {
            return '';
        }
        $readLength = min($length, $available);
        $string = $this->state->bodyBuffer->read($this->offset, $readLength);
        $this->offset += strlen($string);
        return $string;
    }

    public function getContents(): string
    {
        $this->ensureOpen();
        // 与 PHP stream 语义一致：从当前游标读到末尾，因此先确保底层读完整。
        $this->fillAll();
        $available = $this->state->bodyBuffer->getLength() - $this->offset;
        if ($available <= 0) {
            return '';
        }
        $string = $this->state->bodyBuffer->read($this->offset, $available);
        $this->offset += strlen($string);
        return $string;
    }

    public function getMetadata(mixed $key = null): mixed
    {
        $metadata = [
            'streaming' => true,
            'complete' => $this->state->complete,
            'replayable' => true,
            'buffered_bytes' => $this->state->bodyBuffer->getLength(),
            'source_eof' => $this->state->complete,
        ];
        if ($key === null) {
            return $metadata;
        }
        if (!is_string($key)) {
            return null;
        }
        return $metadata[strtolower($key)] ?? $metadata[$key] ?? null;
    }

    public function toString(): string
    {
        $this->ensureOpen();
        // toString 必须稳定、可重复，直接返回完整缓存内容。
        $this->fillAll();
        return (string) $this->state->bodyBuffer;
    }
}
