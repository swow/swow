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

namespace Swow\Tests\Psr7\Server;

use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Sync\WaitReference;

use const CURLOPT_HEADER;
use const CURLOPT_RETURNTRANSFER;
use const CURLOPT_SSL_VERIFYHOST;
use const CURLOPT_SSL_VERIFYPEER;
use const CURLOPT_URL;

/**
 * @internal
 * @coversNothing
 */
final class ServerConnectionTest extends TestCase
{
    public function testSendHttpFile(): void
    {
        $logoPath = __DIR__ . '/swow.svg';
        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $logoPath, $wr): void {
            $connection = $server->acceptConnection();
            $connection->recvHttpRequest();
            $response = Psr7::createResponse();
            $connection->sendHttpFile($response, $logoPath);
        });

        $url = sprintf('%s:%s', $server->getSockAddress(), $server->getSockPort());
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
        curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
        curl_setopt($ch, CURLOPT_HEADER, true);
        $response = curl_exec($ch);
        curl_close($ch);

        $logoSize = filesize($logoPath);
        [$headers, $body] = explode("\r\n\r\n", $response);
        $expected =
            "HTTP/1.1 200 OK\r\n" .
            "Content-Type: image/svg+xml\r\n" .
            "Content-Length: {$logoSize}";
        $this->assertEquals($expected, $headers);
        $this->assertEquals(file_get_contents($logoPath), $body);

        $wr::wait($wr);
        $server->close();
    }
}
