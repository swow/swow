<?php

declare(strict_types=1);

namespace Swow\Http\Protocol;

final class H2PendingFrameQueue
{
    /** @var list<H2Frame> */
    private array $controlFrames = [];

    /** @var list<H2Frame> */
    private array $requestStartFrames = [];

    /** @var array<int, list<H2Frame>> */
    private array $streamFrames = [];

    /** @var list<int> */
    private array $streamOrder = [];

    public function defer(H2Frame $frame): void
    {
        if ($this->isControlFrame($frame)) {
            $this->controlFrames[] = $frame;

            return;
        }

        if ($frame->type === H2Frame::TYPE_HEADERS) {
            $this->requestStartFrames[] = $frame;

            return;
        }

        $this->streamFrames[$frame->streamId] ??= [];
        $this->streamFrames[$frame->streamId][] = $frame;
        $this->trackStream($frame->streamId);
    }

    public function shift(): ?H2Frame
    {
        $frame = array_shift($this->controlFrames);
        if ($frame instanceof H2Frame) {
            return $frame;
        }

        $frame = array_shift($this->requestStartFrames);
        if ($frame instanceof H2Frame) {
            return $frame;
        }

        return $this->shiftStreamFrame();
    }

    public function takeControlFrame(callable $isControlFrame): ?H2Frame
    {
        $frame = array_shift($this->controlFrames);

        return $frame instanceof H2Frame ? $frame : null;
    }

    public function takeRequestStartFrame(): ?H2Frame
    {
        $frame = array_shift($this->requestStartFrames);

        return $frame instanceof H2Frame ? $frame : null;
    }

    public function hasRequestStartFrameForStream(int $streamId): bool
    {
        foreach ($this->requestStartFrames as $frame) {
            if ($frame->streamId === $streamId) {
                return true;
            }
        }

        return false;
    }

    public function takeFrameForStream(int $streamId, callable $isControlFrame): ?H2Frame
    {
        $frame = array_shift($this->controlFrames);
        if ($frame instanceof H2Frame) {
            return $frame;
        }

        $frames = $this->streamFrames[$streamId] ?? [];
        $frame = array_shift($frames);
        if ($frame instanceof H2Frame) {
            $this->storeStreamFrames($streamId, $frames);

            return $frame;
        }

        return null;
    }

    private function shiftStreamFrame(): ?H2Frame
    {
        while ($this->streamOrder !== []) {
            $streamId = array_shift($this->streamOrder);
            if (!is_int($streamId)) {
                continue;
            }

            $frames = $this->streamFrames[$streamId] ?? [];
            $frame = array_shift($frames);
            if (!$frame instanceof H2Frame) {
                unset($this->streamFrames[$streamId]);
                continue;
            }

            $this->storeStreamFrames($streamId, $frames);

            return $frame;
        }

        return null;
    }

    /** @param list<H2Frame> $frames */
    private function storeStreamFrames(int $streamId, array $frames): void
    {
        if ($frames === []) {
            unset($this->streamFrames[$streamId]);

            return;
        }

        $this->streamFrames[$streamId] = $frames;
        $this->streamOrder[] = $streamId;
    }

    private function trackStream(int $streamId): void
    {
        if (isset($this->streamFrames[$streamId]) && count($this->streamFrames[$streamId]) === 1) {
            $this->streamOrder[] = $streamId;
        }
    }

    private function isControlFrame(H2Frame $frame): bool
    {
        return match ($frame->type) {
            H2Frame::TYPE_SETTINGS,
            H2Frame::TYPE_PING,
            H2Frame::TYPE_GOAWAY,
            H2Frame::TYPE_WINDOW_UPDATE,
            H2Frame::TYPE_RST_STREAM => true,
            default => false,
        };
    }
}
