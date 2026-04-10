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

/**
 * @internal
 */
#[CoversClass(H2Connection::class)]
#[CoversClass(H2Frame::class)]
final class H2FlowControlTest extends TestCase
{
    public function testReadRequestBodySendsWindowUpdatesAfterThreshold(): void
    {
        $threshold = H2Connection::DEFAULT_WINDOW_UPDATE_THRESHOLD;
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];
            /** @var list<H2Frame> */
            public array $sentFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->sentFrames[] = $frame;
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_DATA, 0, 1, str_repeat('a', $threshold)),
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, ''),
        ];

        $body = $connection->readRequestBody(1);

        $this->assertSame(str_repeat('a', $threshold), $body);
        $this->assertCount(2, $connection->sentFrames);
        $this->assertSame(H2Frame::TYPE_WINDOW_UPDATE, $connection->sentFrames[0]->type);
        $this->assertSame(0, $connection->sentFrames[0]->streamId);
        $this->assertSame(H2Frame::TYPE_WINDOW_UPDATE, $connection->sentFrames[1]->type);
        $this->assertSame(1, $connection->sentFrames[1]->streamId);
    }

    public function testReadRequestBodyRejectsWhenConnectionWindowExhausted(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->connectionReceiveWindow = 2;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'abc'),
        ];

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Connection receive window exhausted');

        $connection->readRequestBody(1);
    }

    public function testReadRequestBodyRejectsWhenStreamWindowExhausted(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];

            public function __construct(Socket $socket)
            {
                parent::__construct($socket);
                $this->streamReceiveWindows[1] = 2;
            }

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, 'abc'),
        ];

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Stream receive window exhausted');

        $connection->readRequestBody(1);
    }

    public function testEmptyDataDoesNotTriggerWindowUpdate(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            /** @var list<H2Frame> */
            public array $frames = [];
            /** @var list<H2Frame> */
            public array $sentFrames = [];

            public function readFrame(?int $timeout = null): H2Frame
            {
                $frame = array_shift($this->frames);
                if (!$frame instanceof H2Frame) {
                    throw new \RuntimeException('No frame queued');
                }

                return $frame;
            }

            public function sendFrame(H2Frame $frame, ?int $timeout = null): void
            {
                $this->sentFrames[] = $frame;
            }
        };
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1, ''),
        ];

        $body = $connection->readRequestBody(1);

        $this->assertSame('', $body);
        $this->assertCount(0, $connection->sentFrames);
    }

    public function testReadRequestBodyUsesUpdatedInitialWindowSizeForNewStream(): void
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

                return $frame;
            }
        };
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 4)));
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 3, 'abcd'),
        ];

        $body = $connection->readRequestBody(3);

        $this->assertSame('abcd', $body);
    }

    public function testReadRequestBodyRejectsPayloadExceedingUpdatedInitialWindowSize(): void
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

                return $frame;
            }
        };
        $connection->validateFrame(new H2Frame(H2Frame::TYPE_SETTINGS, 0, 0, pack('nN', 0x4, 2)));
        $connection->frames = [
            new H2Frame(H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 5, 'abc'),
        ];

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Stream receive window exhausted');

        $connection->readRequestBody(5);
    }

    public function testWindowUpdateMakesStreamSendWindowReadableForSubsequentLogic(): void
    {
        $connection = new class(new Socket(Socket::TYPE_TCP)) extends H2Connection {
            public function exposeStreamSendWindow(int $streamId): int
            {
                return $this->streamSendWindows[$streamId] ?? self::DEFAULT_INITIAL_WINDOW_SIZE;
            }
        };

        $connection->validateFrame(new H2Frame(H2Frame::TYPE_WINDOW_UPDATE, 0, 7, pack('N', 512)));

        $this->assertSame(H2Connection::DEFAULT_INITIAL_WINDOW_SIZE + 512, $connection->exposeStreamSendWindow(7));
    }
}
