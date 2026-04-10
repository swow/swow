<?php

declare(strict_types=1);

namespace Swow\Http\Protocol;

use Throwable;

final class H2FrameScheduler
{
    /** @var callable(H2Frame): bool */
    private mixed $isControlFrame;

    /**
     * @param callable(H2Frame): bool $isControlFrame
     */
    public function __construct(
        private H2PendingFrameQueue $pendingFrames,
        callable $isControlFrame,
    ) {
        $this->isControlFrame = $isControlFrame;
    }

    public function defer(H2Frame $frame): void
    {
        $this->pendingFrames->defer($frame);
    }

    /**
     * @param callable(?int): H2Frame $readFrame
     */
    public function nextPendingFrame(callable $readFrame, ?int $timeout = null): H2Frame
    {
        $pendingFrame = $this->pendingFrames->shift();
        if ($pendingFrame instanceof H2Frame) {
            return $pendingFrame;
        }

        return $readFrame($timeout);
    }

    public function takePendingControlFrame(): ?H2Frame
    {
        return $this->pendingFrames->takeControlFrame($this->isControlFrame);
    }

    public function takePendingRequestStartFrame(): ?H2Frame
    {
        return $this->pendingFrames->takeRequestStartFrame();
    }

    public function takePendingFrameForStream(int $streamId): ?H2Frame
    {
        return $this->pendingFrames->takeFrameForStream($streamId, $this->isControlFrame);
    }

    public function hasPendingRequestStartFrameForStream(int $streamId): bool
    {
        return $this->pendingFrames->hasRequestStartFrameForStream($streamId);
    }

    /**
     * @param callable(?int): H2Frame $readFrame
     * @param callable(H2Frame, ?int): void $pumpControlFrame
     * @param callable(): bool $canAcceptNewRequests
     * @param callable(Throwable): bool $isGracefulTermination
     */
    public function nextRequestStartFrame(
        callable $readFrame,
        callable $pumpControlFrame,
        callable $canAcceptNewRequests,
        callable $isGracefulTermination,
        ?int $timeout = null,
    ): ?H2Frame {
        if (!$canAcceptNewRequests()) {
            return null;
        }

        while (true) {
            $pendingControlFrame = $this->takePendingControlFrame();
            if ($pendingControlFrame instanceof H2Frame) {
                $pumpControlFrame($pendingControlFrame, $timeout);
                if (!$canAcceptNewRequests()) {
                    return null;
                }

                continue;
            }

            $pendingRequestStartFrame = $this->takePendingRequestStartFrame();
            if ($pendingRequestStartFrame instanceof H2Frame) {
                return $pendingRequestStartFrame;
            }

            try {
                $frame = $readFrame($timeout);
            } catch (Throwable $exception) {
                if ($isGracefulTermination($exception)) {
                    return null;
                }

                throw $exception;
            }

            if ($frame->type === H2Frame::TYPE_HEADERS) {
                return $frame;
            }

            if ($this->isControlFrame($frame)) {
                $pumpControlFrame($frame, $timeout);
            } else {
                $this->defer($frame);
            }

            if (!$canAcceptNewRequests()) {
                return null;
            }
        }
    }

    /**
     * @param callable(?int): H2Frame $readFrame
     */
    public function nextRelevantFrameForStream(
        callable $readFrame,
        int $streamId,
        ?int $timeout = null,
    ): H2Frame {
        $pendingFrame = $this->takePendingFrameForStream($streamId);
        if ($pendingFrame instanceof H2Frame) {
            return $pendingFrame;
        }

        while (true) {
            $frame = $readFrame($timeout);
            if ($frame->streamId === $streamId || $this->isControlFrame($frame)) {
                return $frame;
            }

            $this->defer($frame);
        }
    }

    private function isControlFrame(H2Frame $frame): bool
    {
        return ($this->isControlFrame)($frame);
    }
}
