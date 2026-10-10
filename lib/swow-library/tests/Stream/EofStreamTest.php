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
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Buffer;
use Swow\Coroutine;
use Swow\Errno;
use Swow\SocketException;
use Swow\Stream\EofStream;
use Swow\Stream\MessageTooLargeException;
use Swow\Sync\WaitReference;
use Swow\TestUtils\Testing;

use function str_repeat;
use function strlen;
use function Swow\TestUtils\getRandomBytes;

/**
 * @internal
 */
#[CoversClass(EofStream::class)]
final class EofStreamTest extends TestCase
{
    /** @param Closure(EofStream, EofStream): void $test */
    private function withConnection(Closure $test, string $eof = "\r\n"): void
    {
        $server = new EofStream($eof);
        $server->setMaxMessageLength(1024 * 1024)->bind('127.0.0.1')->listen();
        $peer = new EofStream($eof);
        $connection = null;
        try {
            $peer->setTimeout(1000)->connect($server->getSockAddress(), $server->getSockPort());
            $connection = $server->accept(1000);
            $connection->setTimeout(1000);
            $this->assertSame($eof, $connection->getEof());
            $this->assertSame($server->getMaxMessageLength(), $connection->getMaxMessageLength());
            $test($connection, $peer);
        } finally {
            $connection?->close();
            $peer->close();
            $server->close();
        }
    }

    public function testSocketTypeMustSupportStreams(): void
    {
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Socket type should be a kind of streams');
        new EofStream(type: EofStream::TYPE_UDP);
    }

    public function testChunksSlicesAndCoalescedMessagesUseTheConfiguredDelimiter(): void
    {
        $this->withConnection(function (EofStream $connection, EofStream $peer): void {
            $this->assertSame($peer, $peer->setEof('|END|'));
            $this->assertSame($connection, $connection->setEof('|END|'));
            $peer->sendMessage('prefix:data:suffix', 7, 4);
            $chunk = new Buffer(0);
            $chunk->append("\0b");
            $peer->sendMessageChunks(['a', $chunk, ['-tail', 1, 4]]);
            $peer->sendMessages(['first', ['se', 'cond'], '']);
            foreach (['data', "a\0btail", 'first', 'second', ''] as $expected) {
                $this->assertSame($expected, $connection->recvMessageString());
            }
        });
    }

    #[DataProvider('provideStringReceiveTimeoutDoesNotChangeTheDestinationOffsetCases')]
    public function testStringReceiveTimeoutDoesNotChangeTheDestinationOffset(string $method): void
    {
        $this->withConnection(function (EofStream $connection, EofStream $peer) use ($method): void {
            $peer->sendMessage('body');
            $this->assertSame('body', $connection->{$method}(timeout: 1000));
        });
    }

    /** @return iterable<string, array{string}> */
    public static function provideStringReceiveTimeoutDoesNotChangeTheDestinationOffsetCases(): iterable
    {
        yield 'buffered' => ['recvMessageString'];
        yield 'fast' => ['recvMessageStringFast'];
    }

    public function testFastReceivePreservesThePrefixAcrossMultipleReads(): void
    {
        $this->withConnection(function (EofStream $connection, EofStream $peer): void {
            $body = str_repeat("a\0b", Buffer::COMMON_SIZE);
            $buffer = new Buffer(16);
            $buffer->append('prefix:');
            $writer = Coroutine::run(static function () use ($peer, $body): void {
                $peer->sendMessages([$body, 'next']);
            });
            try {
                $this->assertSame(strlen($body), $connection->recvMessageFast($buffer, timeout: 1000));
                $this->assertSame('prefix:' . $body, (string) $buffer);
                $this->assertSame('next', $connection->recvMessageStringFast(timeout: 1000));
                $this->assertFalse($buffer->isLocked());
            } finally {
                if ($writer->isAlive()) {
                    $writer->kill();
                }
            }
        });
    }

    public function testFastReceiveAtAnExplicitOffsetDiscardsOldTrailingData(): void
    {
        $this->withConnection(function (EofStream $connection, EofStream $peer): void {
            $buffer = new Buffer(64);
            $buffer->append("prefix:obsolete\r\n");
            $peer->sendMessages(['body', 'next']);
            $this->assertSame(4, $connection->recvMessageFast($buffer, offset: strlen('prefix:'), timeout: 1000));
            $this->assertSame('prefix:body', (string) $buffer);
            $this->assertSame('next', $connection->recvMessageStringFast(timeout: 1000));
        });
    }

    public function testBase(): void
    {
        foreach (['recvMessageString', 'recvMessageStringFast'] as $recvMethod) {
            $wr = new WaitReference();
            $server = new EofStream();
            Coroutine::run(function () use ($server, $recvMethod, $wr): void {
                $server->bind('127.0.0.1')->listen();
                try {
                    /* @phpstan-ignore-next-line */
                    while (true) {
                        Coroutine::run(function (EofStream $connection) use ($recvMethod): void {
                            try {
                                /* @phpstan-ignore-next-line */
                                while (true) {
                                    $message = $connection->{$recvMethod}();
                                    $connection->sendMessage($message);
                                }
                            } catch (SocketException $exception) {
                                $this->assertContains($exception->getCode(), [0, Errno::ECONNRESET]);
                            }
                        }, $server->accept());
                    }
                } catch (SocketException $exception) {
                    $this->assertSame(Errno::ECANCELED, $exception->getCode());
                }
            });
            for ($c = 0; $c < Testing::$maxConcurrencyLow; $c++) {
                $wrc = new WaitReference();
                Coroutine::run(function () use ($server, $recvMethod, $wrc): void {
                    $client = new EofStream();
                    $client->connect($server->getSockAddress(), $server->getSockPort());
                    for ($n = 0; $n < Testing::$maxRequestsMid; $n++) {
                        $random = getRandomBytes();
                        $client->sendMessage($random);
                        $response = $client->{$recvMethod}();
                        $this->assertSame($response, $random);
                    }
                    $client->close();
                });
                WaitReference::wait($wrc);
            }
            $server->close();
            WaitReference::wait($wr);
        }
    }

    public function testMaxMessageLength(): void
    {
        foreach (['recvMessageString', 'recvMessageStringFast'] as $recvMethod) {
            $wr = new WaitReference();
            $server = new EofStream();
            Coroutine::run(function () use ($server, $recvMethod, $wr): void {
                $server
                    ->setMaxMessageLength(Testing::$maxLength / 2)
                    ->bind('127.0.0.1')->listen();
                try {
                    $connection = $server->accept();
                    try {
                        $message = $connection->{$recvMethod}();
                        $connection->sendMessage($message);
                    } catch (SocketException $exception) {
                        $this->assertContains($exception->getCode(), [0, Errno::ECONNRESET]);
                    }
                } catch (MessageTooLargeException) {
                    // ignore
                }
            });
            $client = new EofStream();
            $client->connect($server->getSockAddress(), $server->getSockPort());
            $bigMessage = str_repeat('X', Testing::$maxLength);
            $client->sendMessage($bigMessage);
            $response = null;
            try {
                $response = $client->{$recvMethod}();
            } catch (SocketException) {
                // ignore
            }
            $this->assertNull($response);
            $client->close();
            $server->close();
            WaitReference::wait($wr);
        }
    }
}
