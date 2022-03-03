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

namespace SwowTest\Stream;

use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Errno;
use Swow\SocketException;
use Swow\Stream\EofStream;
use Swow\Stream\MessageTooLargeException;
use Swow\Sync\WaitReference;
use function getRandomBytes;
use const TEST_MAX_CONCURRENCY_LOW;
use const TEST_MAX_LENGTH;
use const TEST_MAX_REQUESTS_MID;

/**
 * @internal
 * @coversNothing
 */
final class EofStreamTest extends TestCase
{
    public function testServer(): void
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
                                    $connection->sendMessageString($message);
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
            for ($c = 0; $c < TEST_MAX_CONCURRENCY_LOW; $c++) {
                $wrc = new WaitReference();
                Coroutine::run(function () use ($server, $recvMethod, $wrc): void {
                    $client = new EofStream();
                    $client->connect($server->getSockAddress(), $server->getSockPort());
                    for ($n = 0; $n < TEST_MAX_REQUESTS_MID; $n++) {
                        $random = getRandomBytes();
                        $client->sendMessageString($random);
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
        $wr = new WaitReference();
        $server = new EofStream();
        Coroutine::run(function () use ($server, $wr): void {
            $server
                ->setMaxMessageLength(TEST_MAX_LENGTH / 2)
                ->bind('127.0.0.1')->listen();
            try {
                $connection = $server->accept();
                try {
                    $message = $connection->recvMessageString();
                    $connection->sendMessageString($message);
                } catch (SocketException $exception) {
                    $this->assertContains($exception->getCode(), [0, Errno::ECONNRESET]);
                }
            } catch (MessageTooLargeException) {
                // ignore
            }
        });
        $client = new EofStream();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $random = getRandomBytes(TEST_MAX_LENGTH);
        $client->sendMessageString($random);
        $response = null;
        try {
            $response = $client->recvMessageString();
        } catch (SocketException) {
            // ignore
        }
        $this->assertNull($response);
        $client->close();
        $server->close();
        WaitReference::wait($wr);
    }
}
