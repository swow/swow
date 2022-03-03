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
use Swow\Stream\JsonStream;
use Swow\Sync\WaitReference;
use function getRandomBytes;
use const TEST_MAX_CONCURRENCY_LOW;
use const TEST_MAX_REQUESTS_MID;

/**
 * @internal
 * @coversNothing
 */
final class JsonStreamTest extends TestCase
{
    public function testServer(): void
    {
        $wr = new WaitReference();
        $server = new JsonStream();
        Coroutine::run(function () use ($server, $wr): void {
            $server->bind('127.0.0.1')->listen();
            try {
                /* @phpstan-ignore-next-line */
                while (true) {
                    Coroutine::run(function (JsonStream $connection): void {
                        try {
                            /* @phpstan-ignore-next-line */
                            while (true) {
                                $json = $connection->recvJson();
                                $json = ['result' => $json['query']];
                                $connection->sendJson($json);
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
            Coroutine::run(function () use ($server, $wrc): void {
                $client = new JsonStream();
                $client->connect($server->getSockAddress(), $server->getSockPort());
                for ($n = 0; $n < TEST_MAX_REQUESTS_MID; $n++) {
                    $random = getRandomBytes();
                    $client->sendJson(['query' => $random]);
                    $response = $client->recvJson();
                    $this->assertSame($response['result'], $random);
                }
                $client->close();
            });
            WaitReference::wait($wrc);
        }
        $server->close();
        WaitReference::wait($wr);
    }
}
