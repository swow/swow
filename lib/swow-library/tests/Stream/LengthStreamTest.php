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

namespace Swow\Tests\Stream;

use Closure;
use InvalidArgumentException;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Buffer;
use Swow\Pack\Format;
use Swow\Socket;
use Swow\Stream\LengthStream;
use Swow\Stream\MessageTooLargeException;

use const PHP_INT_MAX;

/** @internal */
#[CoversClass(LengthStream::class)]
#[CoversClass(MessageTooLargeException::class)]
final class LengthStreamTest extends TestCase
{
    /** @param Closure(LengthStream, Socket): void $test */
    private function withConnection(Closure $test): void
    {
        $server = new LengthStream(Format::UINT16_BE);
        $server->setMaxMessageLength(1024)->bind('127.0.0.1')->listen();
        $peer = new Socket(Socket::TYPE_TCP);
        $connection = null;
        try {
            $peer->setTimeout(1000)->connect($server->getSockAddress(), $server->getSockPort());
            $connection = $server->accept(1000);
            $connection->setTimeout(1000);
            $this->assertSame(Format::UINT16_BE, $connection->getFormat());
            $this->assertSame(2, $connection->getFormatSize());
            $this->assertSame(1024, $connection->getMaxMessageLength());
            $test($connection, $peer);
        } finally {
            $connection?->close();
            $peer->close();
            $server->close();
        }
    }

    public function testCoalescedMessagesRetainNextFrame(): void
    {
        $this->withConnection(function (LengthStream $connection, Socket $peer): void {
            $peer->send(pack('n', 3) . 'one' . pack('n', 3) . 'two');
            $this->assertSame('one', $connection->recvMessageString());
            $this->assertSame('two', $connection->recvMessageString());
        });
    }

    public function testReceivePreservesBufferPrefix(): void
    {
        $this->withConnection(function (LengthStream $connection, Socket $peer): void {
            $peer->send(pack('n', 4) . 'data');
            $buffer = new Buffer(0);
            $buffer->append('prefix:');
            $this->assertSame(4, $connection->recvMessage($buffer));
            $this->assertSame('prefix:data', (string) $buffer);
            $this->assertFalse($buffer->isLocked());
        });
    }

    public function testSendChunksCountsActualPayload(): void
    {
        $this->withConnection(function (LengthStream $connection, Socket $peer): void {
            $this->assertSame($connection, $connection->sendMessageChunks(['ab', ['cdef', 1, 2], ['gh'], ['ijk', 1], null]));
            $this->assertSame(pack('n', 8) . 'abdeghjk', $peer->readString(10));
        });
    }

    public function testMaximumMessageLength(): void
    {
        $this->withConnection(function (LengthStream $connection, Socket $peer): void {
            $connection->setMaxMessageLength(-1);
            $this->assertSame(PHP_INT_MAX, $connection->getMaxMessageLength());
            $connection->setMaxMessageLength(2);
            $peer->send(pack('n', 3) . 'abc');
            $this->expectException(MessageTooLargeException::class);
            $this->expectExceptionMessage('Message length 3 exceeds the max length 2');
            $connection->recvMessageString();
        });
    }

    public function testRejectsDatagramSocket(): void
    {
        $this->expectException(InvalidArgumentException::class);
        new LengthStream(type: Socket::TYPE_UDP);
    }
}
