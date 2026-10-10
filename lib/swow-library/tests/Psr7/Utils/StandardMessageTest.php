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

namespace Swow\Tests\Psr7\Utils;

use Nyholm\Psr7\Factory\Psr17Factory as NyholmFactory;
use Nyholm\Psr7\Request as NyholmRequest;
use Nyholm\Psr7\Response as NyholmResponse;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Psr7;

/** @internal */
#[CoversClass(Psr7::class)]
final class StandardMessageTest extends TestCase
{
    public function testStandardResponseVectorsAndStringsPreserveWireData(): void
    {
        $body = "response\0body";
        $response = new NyholmResponse(202, ['X-Result' => ['one', 'two']], $body, '1.0', 'Queued');
        $header = "HTTP/1.0 202 Queued\r\nX-Result: one\r\nX-Result: two\r\n\r\n";
        $this->assertSame([$header, $body], Psr7::convertResponseToVector($response));
        $this->assertSame($header . $body, Psr7::stringifyResponse($response));
        $this->assertSame($header, Psr7::stringifyResponse($response, true));
        $this->assertSame(202, $response->getStatusCode());
        $this->assertSame(['one', 'two'], $response->getHeader('X-Result'));
    }

    public function testStandardRequestStringsPreserveMethodHeadersAndBody(): void
    {
        $body = "request\0body";
        $request = new NyholmRequest('POST', '/path?value=a%2Bb', ['Host' => 'example.test', 'X-Request' => 'value'], $body, '1.0');
        $header = "POST /path?value=a%2Bb HTTP/1.0\r\nHost: example.test\r\nX-Request: value\r\n\r\n";
        $this->assertSame($header . $body, Psr7::stringifyRequest($request));
        $this->assertSame($header, Psr7::stringifyRequest($request, true));
        $this->assertSame('POST', $request->getMethod());
        $this->assertSame('/path?value=a%2Bb', $request->getRequestTarget());
        $this->assertSame('value', $request->getHeaderLine('X-Request'));
    }

    public function testHeaderOperatorsRetainImmutableMessages(): void
    {
        $response = new NyholmResponse(200, ['X-Keep' => 'retained', 'X-Change' => 'original']);
        foreach (['setHeaders', 'withHeaders'] as $method) {
            $changed = Psr7::$method($response, ['x-change' => ['first', 'second'], 'X-New' => 'new']);
            $this->assertNotSame($response, $changed);
            $this->assertSame('original', $response->getHeaderLine('X-Change'));
            $this->assertFalse($response->hasHeader('X-New'));
            $this->assertSame(['first', 'second'], $changed->getHeader('X-Change'));
            $this->assertSame('new', $changed->getHeaderLine('X-New'));
            $this->assertSame('retained', $changed->getHeaderLine('X-Keep'));
        }
    }

    public function testBodyAndStatusOperatorsRetainImmutableMessages(): void
    {
        $factory = new NyholmFactory();
        $response = new NyholmResponse(200, [], 'original');
        $stream = $factory->createStream("changed\0body");
        foreach (['setBody', 'withBody'] as $method) {
            $changed = Psr7::$method($response, $stream);
            $this->assertNotSame($response, $changed);
            $this->assertSame('original', (string) $response->getBody());
            $this->assertSame($stream, $changed->getBody());
            $this->assertSame("changed\0body", (string) $changed->getBody());
        }
        $changed = Psr7::setStatus($response, 202, 'Queued');
        $this->assertNotSame($response, $changed);
        $this->assertSame(200, $response->getStatusCode());
        $this->assertSame(202, $changed->getStatusCode());
        $this->assertSame('Queued', $changed->getReasonPhrase());
        $this->assertSame('original', (string) $changed->getBody());
    }
}
