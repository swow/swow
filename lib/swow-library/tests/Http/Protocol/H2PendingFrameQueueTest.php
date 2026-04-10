<?php

declare(strict_types=1);

namespace Swow\Tests\Http\Protocol;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Http\Protocol\H2Frame;
use Swow\Http\Protocol\H2PendingFrameQueue;

#[CoversClass(H2PendingFrameQueue::class)]
final class H2PendingFrameQueueTest extends TestCase
{
    public function testTakeControlFramePrioritizesControlFrames(): void
    {
        $queue = new H2PendingFrameQueue();
        $request = new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 3, 'h');
        $control = new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1));

        $queue->defer($request);
        $queue->defer($control);

        $this->assertSame($control, $queue->takeControlFrame(static fn(H2Frame $frame): bool => true));
        $this->assertSame($request, $queue->takeRequestStartFrame());
    }

    public function testTakeFrameForStreamReturnsControlFramesBeforeStreamFrames(): void
    {
        $queue = new H2PendingFrameQueue();
        $streamFrame = new H2Frame(H2Frame::TYPE_DATA, 0, 5, 'abc');
        $control = new H2Frame(H2Frame::TYPE_PING, 0, 0, '12345678');

        $queue->defer($streamFrame);
        $queue->defer($control);

        $this->assertSame($control, $queue->takeFrameForStream(5, static fn(H2Frame $frame): bool => true));
        $this->assertSame($streamFrame, $queue->takeFrameForStream(5, static fn(H2Frame $frame): bool => true));
    }

    public function testShiftReturnsControlThenRequestStartThenStreamFrames(): void
    {
        $queue = new H2PendingFrameQueue();
        $request = new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 3, 'h');
        $streamFrame = new H2Frame(H2Frame::TYPE_DATA, 0, 3, 'abc');
        $control = new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, 0));

        $queue->defer($streamFrame);
        $queue->defer($request);
        $queue->defer($control);

        $this->assertSame($control, $queue->shift());
        $this->assertSame($request, $queue->shift());
        $this->assertSame($streamFrame, $queue->shift());
        $this->assertNull($queue->shift());
    }

    public function testShiftRoundsRobinAcrossDeferredStreamFrames(): void
    {
        $queue = new H2PendingFrameQueue();
        $streamThreeA = new H2Frame(H2Frame::TYPE_DATA, 0, 3, 'a');
        $streamThreeB = new H2Frame(H2Frame::TYPE_DATA, 0, 3, 'b');
        $streamFive = new H2Frame(H2Frame::TYPE_DATA, 0, 5, 'c');

        $queue->defer($streamThreeA);
        $queue->defer($streamThreeB);
        $queue->defer($streamFive);

        $this->assertSame($streamThreeA, $queue->shift());
        $this->assertSame($streamFive, $queue->shift());
        $this->assertSame($streamThreeB, $queue->shift());
        $this->assertNull($queue->shift());
    }
}
