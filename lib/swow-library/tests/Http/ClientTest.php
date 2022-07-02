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

namespace SwowTest\Http;

use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Http\Client as HttpClient;
use Swow\Http\Request as HttpRequest;
use Swow\Http\Status;
use Swow\Socket;
use Swow\Sync\WaitReference;
use function usleep;

/**
 * @internal
 * @coversNothing
 */
final class ClientTest extends TestCase
{
    /** This causes HttpParser fall into dead-loop before */
    public function testBadNetwork(): void
    {
        $server = new Socket(Socket::TYPE_TCP);
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $wr): void {
            $connection = $server->accept();
            $connection->recvString();
            $connection->sendString("HTTP/1.1 200 OK\r\nContent-Length: ");
            usleep(10 * 1000);
            $connection->sendString("0\r\nConnection");
            usleep(10 * 1000);
            $connection->sendString(": closed\r\n\r\n");
        });

        $client = new HttpClient();
        $client->connect($server->getSockAddress(), $server->getSockPort());
        $response = $client->sendRequest(new HttpRequest());
        $this->assertSame($response->getStatusCode(), Status::OK);

        $wr::wait($wr);
    }
}
