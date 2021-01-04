<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace SwowTest\Http;

use PHPUnit\Framework\TestCase;
use Swow\Http\Buffer;
use Swow\Http\ConfigTrait;
use Swow\Http\Exception;
use Swow\Http\Parser;
use Swow\Http\ReceiverTrait;

/**
 * @internal
 * @coversNothing
 */
class ClientTest extends TestCase
{
    protected $badContent = 'HTTP/1.1 400 Bad Request: missing required Host header
Content-Type: text/plain; charset=utf-8
Connection: close

400 Bad Request: missing required Host header';

    protected $content400 = 'HTTP/1.1 400 Bad Request: missing required Host header
Content-Type: text/plain; charset=utf-8
Connection: close
Content-Length: 45

400 Bad Request: missing required Host header';

    protected $content200 = 'HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Content-Length: 12

Hello World.';

    protected $content200Close = 'HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Connection: close
Content-Length: 12

Hello World.';

    public function testExecute()
    {
        $client = $this->getClient([$this->content200, $this->content200Close]);
        $raw = $client->publicExecute();
        $this->assertSame(200, $raw->statusCode);
        $this->assertSame('OK', $raw->reasonPhrase);
        $this->assertSame(['Content-Type' => ['text/plain; charset=utf-8'], 'Content-Length' => ['12']], $raw->headers);
        $this->assertSame(['content-type' => 'Content-Type', 'content-length' => 'Content-Length'], $raw->headerNames);
        $this->assertSame('1.1', $raw->protocolVersion);
        $this->assertSame('Hello World.', (string) $raw->body);
        $this->assertInstanceOf(Buffer::class, $raw->body);
        $this->assertSame(12, $raw->contentLength);
        $this->assertTrue($raw->shouldKeepAlive);
        $this->assertFalse($raw->isUpgrade);

        $raw = $client->publicExecute();
        $this->assertFalse($raw->shouldKeepAlive);
    }

    public function testUnexpectedBodyEof()
    {
        try {
            $this->getClient([$this->badContent])->publicExecute();
        } catch (Exception $exception) {
            $this->assertSame(400, $exception->getCode());
            $this->assertSame('Protocol Parsing Error', $exception->getMessage());
            $rawData = $exception->getRawData();
            $this->assertSame(400, $rawData->statusCode);
            $this->assertSame(['text/plain; charset=utf-8'], $rawData->headers['Content-Type']);
            $this->assertSame('400 Bad Request: missing required Host header', (string) $rawData->body);
            $this->assertSame('Bad Request: missing required Host header', $rawData->reasonPhrase);
        }
    }

    public function testRecvAgainWhenParseError()
    {
        try {
            $client = $this->getClient([$this->badContent, $this->content400]);
            $client->publicExecute();
        } catch (Exception $exception) {
            $raw = $client->publicExecute();
        }

        $this->assertSame(400, $raw->statusCode);
        $this->assertSame('400 Bad Request: missing required Host header', (string) $raw->body);
    }

    protected function getClient($contents = [])
    {
        return new class($contents) {
            use ReceiverTrait {
                __construct as receiverConstruct;
            }
            use ConfigTrait;

            protected $contents;

            public function __construct($contents)
            {
                $this->contents = $contents;
                $this->receiverConstruct(Parser::TYPE_RESPONSE, Parser::EVENTS_ALL);
            }

            public function publicExecute()
            {
                return $this->execute($this->getMaxHeaderLength(), $this->getMaxContentLength());
            }

            public function recvData(Buffer $buffer, ?int $size = null, ?int $timeout = null): int
            {
                $content = array_shift($this->contents);
                $buffer->write($content);
                $buffer->rewind();

                return strlen($content);
            }
        };
    }
}
