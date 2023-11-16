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
use stdClass;
use Swow\Coroutine;
use Swow\Errno;
use Swow\SocketException;
use Swow\Stream\VarStream;
use Swow\Sync\WaitReference;
use Swow\TestUtils\Testing;

use function Swow\TestUtils\getRandomBytes;

/**
 * @internal
 * @covers \Swow\Stream\VarStream
 */
final class VarStreamTest extends TestCase
{
    public function testServer(): void
    {
        $wr = new WaitReference();
        $server = new VarStream();
        Coroutine::run(function () use ($server, $wr): void {
            $server->bind('127.0.0.1')->listen();
            try {
                /* @phpstan-ignore-next-line */
                while (true) {
                    Coroutine::run(function (VarStream $connection): void {
                        try {
                            /* @phpstan-ignore-next-line */
                            while (true) {
                                $request = (object) $connection->recvVar();
                                $response = new stdClass();
                                $response->result = $request->query;
                                $connection->sendVar($response);
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
            Coroutine::run(function () use ($server, $wrc): void {
                $client = new VarStream();
                $client->connect($server->getSockAddress(), $server->getSockPort());
                for ($n = 0; $n < Testing::$maxRequestsMid; $n++) {
                    $random = getRandomBytes();
                    $request = new stdClass();
                    $request->query = $random;
                    $client->sendVar($request);
                    $response = (object) $client->recvVar();
                    $this->assertSame($response->result, $random);
                }
                $client->close();
            });
            WaitReference::wait($wrc);
        }
        $server->close();
        WaitReference::wait($wr);
    }
}
