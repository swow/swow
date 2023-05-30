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
use Swow\Http\Status;
use Swow\Psr7\Psr7;
use Swow\Psr7\Server\Server;
use Swow\Sync\WaitReference;

use function array_map;
use function is_numeric;
use function Swow\TestUtils\getRandomBytes;

use const CURLOPT_HEADER;
use const CURLOPT_PROXY;
use const CURLOPT_RETURNTRANSFER;
use const CURLOPT_URL;

/**
 * @internal
 * @covers \Swow\Psr7\Server\ServerConnection::sendHttpFile()
 */
final class ServerConnectionTest extends TestCase
{
    protected string $tempFile = '';

    protected string $tempDir = '';

    protected function setUp(): void
    {
        $tempDir = __DIR__ . '/temp';

        if (!is_dir($tempDir)) {
            mkdir($tempDir);
        }

        $this->tempDir = $tempDir;

        $fileName = uniqid() . '.txt';
        $this->tempFile = $tempDir . '/' . $fileName;
        file_put_contents($this->tempFile, getRandomBytes(8192));
    }

    protected function tearDown(): void
    {
        unlink($this->tempFile);
        rmdir($this->tempDir);
        $this->tempFile = '';
        $this->tempDir = '';
    }

    public function testSendHttpFile(): void
    {
        $server = new Server();
        $server->bind('127.0.0.1')->listen();

        $tempFile = $this->tempFile;
        $wr = new WaitReference();
        Coroutine::run(static function () use ($server, $tempFile, $wr): void {
            $connection = $server->acceptConnection();
            $connection->recvHttpRequest();
            $response = Psr7::createResponse();
            $connection->sendHttpFile($response, $tempFile);
        });

        $url = sprintf('%s:%s', $server->getSockAddress(), $server->getSockPort());
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
        curl_setopt($ch, CURLOPT_HEADER, true);
        curl_setopt($ch, CURLOPT_PROXY, false);
        $response = curl_exec($ch);
        curl_close($ch);

        $fileSize = filesize($this->tempFile);
        [$headerLines, $body] = explode("\r\n\r\n", $response);
        // parse headers to array
        $headerLines = explode("\r\n", $headerLines);
        $headLine = array_shift($headerLines);
        $headLine = explode(' ', $headLine, 3);
        if (!isset($headLine[1]) || !is_numeric($headLine[1])) {
            $this->fail('Invalid response');
        }
        $status = (int) $headLine[1];
        $headers = [];
        foreach ($headerLines as $headerLine) {
            $headerLineParts = array_map('trim', explode(':', $headerLine, 2));
            $headers[$headerLineParts[0]] = $headerLineParts[1] ?? '';
        }
        $this->assertSame(Status::OK, $status);

        $this->assertSame('text/plain', $headers['Content-Type'] ?? '');
        $this->assertSame($fileSize, (int) ($headers['Content-Length'] ?? ''));
        $this->assertSame(file_get_contents($this->tempFile), $body);

        $wr::wait($wr);
        $server->close();
    }
}
