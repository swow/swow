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

    public function testUnexpectedBodyEof()
    {
        $this->expectException(Exception::class);
        $this->expectErrorMessageMatches('/^Protocol Parsing Error, The received data is /');

        $this->getClient([$this->badContent])->publicExecute();
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
