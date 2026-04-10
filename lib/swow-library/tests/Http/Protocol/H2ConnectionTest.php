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

namespace Swow\Tests\Http\Protocol;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Http\Protocol\H2Connection;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;
use Swow\Socket;

use function pack;

/**
 * @internal
 */
#[CoversClass(H2Connection::class)]
final class H2ConnectionTest extends TestCase
{
    public function testValidateFrameAcceptsEmptySettingsAck(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));
        $frame = new H2Frame(H2Frame::TYPE_SETTINGS, H2Frame::FLAG_ACK, 0, '');

        $validated = $connection->validateFrame($frame);

        $this->assertSame($frame, $validated);
    }

    public function testValidateFrameRejectsSettingsAckWithPayload(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('SETTINGS ACK frame must not contain a payload');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, H2Frame::FLAG_ACK, 0, 'x'));
    }

    public function testValidateFrameRejectsPingPayloadLengthNotEqualToEight(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('PING payload length must be exactly 8');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_PING, 0, 0, 'short'));
    }

    public function testValidateFrameRejectsGoAwayPayloadShorterThanEight(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('GOAWAY payload length must be at least 8');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, 'short'));
    }

    public function testValidateFrameRejectsWindowUpdateIncrementZero(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('WINDOW_UPDATE increment must not be 0');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 0)));
    }

    public function testValidateFrameAcceptsValidWindowUpdate(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));
        $frame = new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 1024));

        $validated = $connection->validateFrame($frame);

        $this->assertSame($frame, $validated);
    }

    public function testValidateFrameAppliesInitialWindowSizeSetting(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function exposeStreamReceiveWindow(int $streamId): int
            {
                return $this->streamReceiveWindows[$streamId] ?? $this->streamInitialWindowSize;
            }
        };

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 1024)));

        $this->assertSame(1024, $connection->exposeStreamReceiveWindow(99));
    }

    public function testValidateFrameRejectsInitialWindowSizeGreaterThanAllowed(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('SETTINGS_INITIAL_WINDOW_SIZE exceeds maximum allowed value');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, H2Frame::MAX_WINDOW_SIZE + 1)));
    }

    public function testValidateFrameAppliesMaxFrameSizeSetting(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function exposeLocalMaxFrameSize(): int
            {
                return $this->localMaxFrameSize;
            }
        };

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x5, 32768)));

        $this->assertSame(32768, $connection->exposeLocalMaxFrameSize());
    }

    public function testValidateFrameRejectsInvalidMaxFrameSizeSetting(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('SETTINGS_MAX_FRAME_SIZE out of allowed range');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x5, H2Frame::DEFAULT_MAX_FRAME_SIZE - 1)));
    }

    public function testValidateFrameAppliesConnectionLevelWindowUpdateToSendWindow(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function exposeConnectionSendWindow(): int
            {
                return $this->connectionSendWindow;
            }
        };

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1024)));

        $this->assertSame(H2Connection::DEFAULT_INITIAL_WINDOW_SIZE + 1024, $connection->exposeConnectionSendWindow());
    }

    public function testValidateFrameAppliesStreamLevelWindowUpdateToSendWindow(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function exposeStreamSendWindow(int $streamId): int
            {
                return $this->streamSendWindows[$streamId] ?? self::DEFAULT_INITIAL_WINDOW_SIZE;
            }
        };

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 3, pack('N', 2048)));

        $this->assertSame(H2Connection::DEFAULT_INITIAL_WINDOW_SIZE + 2048, $connection->exposeStreamSendWindow(3));
    }

    public function testValidateFrameRejectsWindowUpdateWhenConnectionSendWindowOverflows(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionSendWindow = H2Frame::MAX_WINDOW_SIZE;
            }
        };

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Connection send window exceeds maximum allowed value');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 0, pack('N', 1)));
    }

    public function testValidateFrameRejectsWindowUpdateWhenStreamSendWindowOverflows(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[5] = H2Frame::MAX_WINDOW_SIZE;
            }
        };

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Stream send window exceeds maximum allowed value');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 5, pack('N', 1)));
    }

    public function testValidateFrameAppliesInitialWindowSizeSettingToNewStreamSendWindow(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function exposeStreamSendWindow(int $streamId): int
            {
                return $this->streamSendWindows[$streamId] ?? $this->streamSendInitialWindowSize;
            }
        };

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 1024)));

        $this->assertSame(1024, $connection->exposeStreamSendWindow(11));
    }

    public function testValidateFrameAdjustsExistingStreamSendWindowsWhenInitialWindowSizeChanges(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[7] = 32_768;
            }

            public function exposeStreamSendWindow(int $streamId): int
            {
                return $this->streamSendWindows[$streamId] ?? $this->streamSendInitialWindowSize;
            }
        };

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 70_000)));

        $this->assertSame(37_233, $connection->exposeStreamSendWindow(7));
    }

    public function testValidateFrameRejectsInitialWindowSizeWhenAdjustedStreamSendWindowOverflows(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamSendWindows[9] = H2Frame::MAX_WINDOW_SIZE;
            }
        };

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Adjusted stream send window exceeds maximum allowed value');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, H2Frame::MAX_WINDOW_SIZE)));
    }

    public function testSendRstStreamWritesExpectedFrame(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->frames[] = $frame;
            }
        };

        $connection->sendRstStream(3, H2Exception::ERROR_CANCEL);

        $this->assertCount(1, $connection->frames);
        $this->assertSame(H2Frame::TYPE_RST_STREAM, $connection->frames[0]->type);
        $this->assertSame(3, $connection->frames[0]->streamId);
        $this->assertSame(pack('N', H2Exception::ERROR_CANCEL), $connection->frames[0]->payload);
    }

    public function testValidateFrameAppliesGoAwayLastAllowedStreamId(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)));

        $this->assertTrue($connection->hasReceivedGoAway());
        $this->assertSame(1, $connection->getLastAllowedStreamId());
    }

    public function testValidateFrameMarksStreamClosedAfterReceivingRstStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_RST_STREAM, 0, 5, pack('N', H2Exception::ERROR_CANCEL)));

        $this->assertTrue($connection->isStreamClosed(5));
    }

    public function testValidateFrameAllowsRepeatedRstStreamForClosedStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_RST_STREAM, 0, 5, pack('N', H2Exception::ERROR_CANCEL)));
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_RST_STREAM, 0, 5, pack('N', H2Exception::ERROR_CANCEL)));

        $this->assertTrue($connection->isStreamClosed(5));
    }

    public function testValidateFrameRejectsWindowUpdateForClosedStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_RST_STREAM, 0, 5, pack('N', H2Exception::ERROR_CANCEL)));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('WINDOW_UPDATE received for closed stream');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 5, pack('N', 1024)));
    }

    public function testValidateFrameAllowsWindowUpdateForHalfClosedRemoteStream(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82"),
        ];

        $connection->readHeaderBlock();
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 1, pack('N', 1024)));

        $this->assertSame(H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE, $connection->getStreamState(1));
        $this->assertSame(H2Connection::DEFAULT_INITIAL_WINDOW_SIZE + 1024, $connection->getStreamSendWindow(1));
    }

    public function testValidateFrameClosesHalfClosedRemoteStreamAfterRstStream(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82"),
        ];

        $connection->readHeaderBlock();
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_RST_STREAM, 0, 1, pack('N', H2Exception::ERROR_CANCEL)));

        $this->assertTrue($connection->isStreamClosed(1));
        $this->assertSame(H2Connection::STREAM_STATE_CLOSED, $connection->getStreamState(1));
    }

    public function testValidateFrameAllowsGoAwayWhileHalfClosedRemoteStateIsPreserved(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82"),
        ];

        $connection->readHeaderBlock();
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_GOAWAY, 0, 0, pack('NN', 1, H2Exception::ERROR_NO_ERROR)));

        $this->assertTrue($connection->hasReceivedGoAway());
        $this->assertSame(H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE, $connection->getStreamState(1));
    }

    public function testValidateFrameRejectsDataForClosedStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_RST_STREAM, 0, 5, pack('N', H2Exception::ERROR_CANCEL)));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Frame received for closed stream');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_DATA, 0, 5, 'a'));
    }

    public function testValidateFrameRejectsHeadersForClosedStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_RST_STREAM, 0, 7, pack('N', H2Exception::ERROR_CANCEL)));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Frame received for closed stream');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 7, "\x82"));
    }

    public function testValidateFrameRejectsHeadersOnEvenRequestStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Request frame must use an odd positive stream ID');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS, 2, "\x82"));
    }

    public function testValidateFrameRejectsDataOnEvenRequestStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Request frame must use an odd positive stream ID');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_DATA, 0, 2, 'a'));
    }

    public function testValidateFrameRejectsContinuationOnStreamZero(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Request frame must use an odd positive stream ID');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_CONTINUATION, 0, 0, 'a'));
    }

    public function testValidateFrameRejectsDataForIdleStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Frame received for idle stream');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'a'));
    }

    public function testValidateFrameRejectsContinuationForIdleStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Frame received for idle stream');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_CONTINUATION, 0, 1, 'a'));
    }

    public function testValidateFrameAllowsDataForExistingRequestStream(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));
        $connection->markRequestStreamProcessed(1);

        $frame = $connection->validateFrame(new H2Frame(H2Frame::TYPE_DATA, 0, 1, 'a'));

        $this->assertSame(H2Frame::TYPE_DATA, $frame->type);
        $this->assertSame(1, $frame->streamId);
    }

    public function testValidateFrameRejectsPriorityOnStreamZero(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('PRIORITY frame must use a positive stream ID');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_PRIORITY, 0, 0, '12345'));
    }

    public function testValidateFrameRejectsPriorityWithInvalidPayloadLength(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('PRIORITY payload length must be exactly 5');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_PRIORITY, 0, 3, '1234'));
    }

    public function testValidateFrameRejectsClientPushPromise(): void
    {
        $connection = new H2Connection(new Socket(Socket::TYPE_TCP));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Client must not send PUSH_PROMISE frames');

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_PUSH_PROMISE, 0, 3, '1234'));
    }

    public function testReadHeaderBlockMarksRemoteHalfClosedWhenEndStreamIsSet(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $this->validateFrame($frame);
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_HEADERS, H2Frame::FLAG_END_HEADERS | H2Frame::FLAG_END_STREAM, 1, "\x82"),
        ];

        $connection->readHeaderBlock();

        $this->assertSame(H2Connection::STREAM_STATE_HALF_CLOSED_REMOTE, $connection->getStreamState(1));
    }
}
