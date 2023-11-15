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

use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Errno;
use Swow\SocketException;
use Swow\Stream\EofStream;
use Swow\Stream\MessageTooLargeException;
use Swow\Sync\WaitReference;
use Swow\TestUtils\Testing;

use function str_repeat;
use function Swow\TestUtils\getRandomBytes;

/**
 * @internal
 * @covers \Swow\Stream\EofStream
 */
final class EofStreamTest extends TestCase
{
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
