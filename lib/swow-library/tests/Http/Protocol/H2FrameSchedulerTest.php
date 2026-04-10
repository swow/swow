<?php

declare(strict_types=1);

namespace Swow\Tests\Http\Protocol;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;
use Swow\Http\Protocol\H2FrameScheduler;
use Swow\Http\Protocol\H2PendingFrameQueue;

#[CoversClass(H2FrameScheduler::class)]
final class H2FrameSchedulerTest extends TestCase
{
    public function testNextRequestStartFramePumpsControlFramesBeforeDeferredRequestStarts(): void
    {
        $scheduler = new H2FrameScheduler(
            new H2PendingFrameQueue(),
            static fn(H2Frame $frame): bool => in_array($frame->type, [H2Frame::TYPE_GOAWAY, H2Frame::TYPE_WINDOW_UPDATE], true)
        );

        $scheduler->defer(new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 3, 'h'));
        $scheduler->defer(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 3, H2Exception::ERROR_NO_ERROR)));

        $pumped = [];
        $frame = $scheduler->nextRequestStartFrame(
            static fn() => throw new \RuntimeException('not expected'),
            static function (H2Frame $frame) use (&$pumped): void {
                $pumped[] = $frame->type;
            },
            static fn(): bool => true,
            static fn(\Throwable $exception): bool => false,
        );

        $this->assertSame([H2Frame::TYPE_GOAWAY], $pumped);
        $this->assertSame(3, $frame?->streamId);
    }

    public function testNextRelevantFrameForStreamDefersDifferentStreamFramesUntilLater(): void
    {
        $scheduler = new H2FrameScheduler(
            new H2PendingFrameQueue(),
            static fn(H2Frame $frame): bool => $frame->type === H2Frame::TYPE_WINDOW_UPDATE
        );
        $frames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 3, 'other'),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'body'),
        ];

        $frame = $scheduler->nextRelevantFrameForStream(
            static function () use (&$frames): H2Frame {
                $frame = array_shift($frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            },
            1
        );

        $this->assertSame(1, $frame->streamId);
        $this->assertSame('body', $frame->payload);
        $this->assertSame(3, $scheduler->takePendingFrameForStream(3)?->streamId);
    }
}
