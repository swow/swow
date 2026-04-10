<?php

declare(strict_types=1);

namespace Swow\Tests\Http\Protocol;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;
use Swow\Http\Protocol\H2StreamStateMatrix;

#[CoversClass(H2StreamStateMatrix::class)]
final class H2StreamStateMatrixTest extends TestCase
{
    public function testAssertCanSendRejectsHalfClosedLocal(): void
    {
        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Cannot send on locally closed stream');

        H2StreamStateMatrix::assertCanSend(1, H2Connection::STREAM_STATE_HALF_CLOSED_LOCAL);
    }

    public function testAssertCanReceiveDataRejectsHalfClosedRemote(): void
    {
        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Cannot receive DATA on remotely closed stream');

        H2StreamStateMatrix::assertCanReceiveData(1, H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE);
    }

    public function testStateTransitionsCloseStreamWhenBothSidesEnded(): void
    {
        $this->assertSame(
            H2Connection::STREAM_STATE_CLOSED,
            H2StreamStateMatrix::stateAfterRemoteEnded(H2Connection::STREAM_STATE_HALF_CLOSED_LOCAL)
        );
        $this->assertSame(
            H2Connection::STREAM_STATE_CLOSED,
            H2StreamStateMatrix::stateAfterLocalEnded(H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE)
        );
    }

    public function testValidateInboundFrameRejectsClosedDataFrame(): void
    {
        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Frame received for closed stream');

        H2StreamStateMatrix::validateInboundFrame(
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'a'),
            H2Connection::STREAM_STATE_CLOSED,
            true,
            false,
            false
        );
    }
}
