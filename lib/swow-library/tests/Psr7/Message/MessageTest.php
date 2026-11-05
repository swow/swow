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

namespace Swow\Tests\Psr7\Message;

use InvalidArgumentException;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Psr\Http\Message\MessageInterface;
use Swow\Http\Message\ResponseEntity;
use Swow\Psr7\Message\AbstractMessage;
use Swow\Psr7\Message\MessagePlusInterface;
use Swow\Psr7\Psr7;

/**
 * @internal
 */
#[CoversClass(AbstractMessage::class)]
#[CoversClass(Psr7::class)]
final class MessageTest extends TestCase
{
    public function testAddHeader(): void
    {
        foreach ([false, true] as $useWith) {
            $request = Psr7::createRequest('GET', '/request', ['foo' => 'bar']);
            if (!$useWith && !($request instanceof MessagePlusInterface)) {
                continue;
            }
            $this->assertSame($request->getHeaders(), ['foo' => ['bar']]);
            if (!$useWith) {
                $request->addHeader('foo', 'baz');
            } else {
                $request = $request->withAddedHeader('foo', 'baz');
            }
            $this->assertSame($request->getHeader('foo'), ['bar', 'baz']);
            if (!$useWith) {
                $request->addHeader('char', 'dua');
            } else {
                $request = $request->withAddedHeader('char', 'dua');
            }
            $this->assertSame($request->getHeader('char'), ['dua']);
            $this->assertSame($request->getHeaderLine('char'), 'dua');
            $this->assertSame($request->getHeaders(), ['foo' => ['bar', 'baz'], 'char' => ['dua']]);
        }
    }

    public function testKeepAliveProtocolSemantics(): void
    {
        $response = Psr7::createResponse(body: 'hello');
        $this->assertStringContainsString("Connection: keep-alive\r\n", $response->toString(true));

        $response->setProtocolVersion('1.0');
        $this->assertStringContainsString("Connection: close\r\n", $response->toString(true));

        $response->setProtocolVersion('2.0');
        $this->assertStringNotContainsString('Connection:', $response->toString(true));

        $response->setProtocolVersion('1.1')->setHeader('Connection', 'foo, close');
        $this->assertFalse($response->shouldKeepAlive());
        $this->assertFalse(Psr7::detectShouldKeepAlive($response));

        $request = Psr7::createRequest('GET', '/');
        $this->assertStringContainsString("Connection: keep-alive\r\n", $request->toString(true));
    }

    public function testInjectedLineBreaksInHeadersAreRejected(): void
    {
        $response = Psr7::createResponse(200);
        foreach (["Injected: x\r\nContent-Length: 0", "Injected\nX: y", "Injected\0"] as $value) {
            try {
                $response->setHeader('Location', $value);
                $this->fail('A header value with a line break must be rejected');
            } catch (InvalidArgumentException) {
            }
            try {
                $response->addHeader('X-Extra', [$value]);
                $this->fail('An array header value with a line break must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
        foreach (['Bad Name', 'Bad:Name', "Bad\r\nName"] as $name) {
            try {
                $response->setHeader($name, 'value');
                $this->fail('An invalid header name must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
        // valid values still work and reach the wire unchanged
        $response->setHeader('X-Valid', 'plain value');
        $this->assertStringContainsString("X-Valid: plain value\r\n", $response->toString(true));
    }

    public function testInjectedLineBreaksInTheRequestLineAreRejected(): void
    {
        $request = Psr7::createRequest('GET', '/');
        try {
            $request->setMethod("GET\r\nX: y");
            $this->fail('A method with a line break must be rejected');
        } catch (InvalidArgumentException) {
        }
        try {
            $request->setRequestTarget("/a\r\nX: y");
            $this->fail('A request target with a line break must be rejected');
        } catch (InvalidArgumentException) {
        }
        $this->assertSame('GET', $request->getMethod());
        $this->assertSame('/', $request->getRequestTarget());
    }

    public function testInjectedLineBreaksInTheStatusLineAreRejected(): void
    {
        $response = Psr7::createResponse(200);
        try {
            $response->setStatus(200, "OK\r\nX: y");
            $this->fail('A reason phrase with a line break must be rejected');
        } catch (InvalidArgumentException) {
        }
        $response->setStatus(200, 'All Good');
        $this->assertStringStartsWith('HTTP/1.1 200 All Good', $response->toString(true));
    }

    public function testDetectShouldKeepAliveForPlainPsrMessage(): void
    {
        $message = $this->createMock(MessageInterface::class);
        $message->method('getProtocolVersion')->willReturn('1.1');
        $message->method('getHeaderLine')->with('Connection')->willReturn('foo, close');

        $this->assertFalse(Psr7::detectShouldKeepAlive($message));
    }

    public function testTrailingLineFeedIsRejectedByTheAnchor(): void
    {
        /* "$" in PCRE also matches before a trailing LF, so the token patterns
         * have to anchor with \z; otherwise these would be accepted */
        $response = Psr7::createResponse(200);
        foreach (["X\n", "X\r"] as $name) {
            try {
                $response->setHeader($name, 'value');
                $this->fail('A header name with a trailing line break must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
        $request = Psr7::createRequest('GET', '/');
        foreach (["GET\n", "GET\r"] as $method) {
            try {
                $request->setMethod($method);
                $this->fail('A method with a trailing line break must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
        /* Nothing was stored: a rejected name must not leave a header behind,
         * and the method must still be the one from the constructor. The count
         * would grow by one if the name were accepted. */
        $this->assertSame([], $response->getHeaders());
        $this->assertSame('GET', $request->getMethod());
    }

    public function testHeaderValueAcceptsOnlyTheLegalControlCharacter(): void
    {
        $response = Psr7::createResponse(200);
        // HTAB is part of a field value
        $response->setHeader('X-Tab', "a\tb");
        $this->assertSame("a\tb", $response->getHeaderLine('X-Tab'));
        // obs-text is part of a field value
        $response->setHeader('X-Obs', "a\x80b");
        $this->assertSame("a\x80b", $response->getHeaderLine('X-Obs'));
        // every other control character is not
        foreach (["\x00", "\x01", "\x0b", "\x0c", "\x1f", "\x7f"] as $control) {
            try {
                $response->setHeader('X-Bad', "a{$control}b");
                $this->fail('A control character other than HTAB must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
    }

    public function testRequestTargetForms(): void
    {
        $request = Psr7::createRequest('GET', '/');
        // one accepted value per request-target form
        foreach (['/a?b=1', 'http://example.com/a', 'example.com:443', '*'] as $target) {
            $request->setRequestTarget($target);
            $this->assertSame($target, $request->getRequestTarget());
        }
        // no form allows an empty target, a fragment, a backslash or obs-text
        foreach (['', '/a#b', '/a\b', "/a\x80b"] as $target) {
            try {
                $request->setRequestTarget($target);
                $this->fail('An invalid request target must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
    }

    public function testReasonPhraseAndStatusCode(): void
    {
        $response = Psr7::createResponse(200);
        // HTAB and obs-text are legal in a reason phrase
        $response->setStatus(200, "All\tGood");
        $this->assertSame("All\tGood", $response->getReasonPhrase());
        // "0" is a legal phrase and must not be replaced by the default
        $response->setStatus(200, '0');
        $this->assertSame('0', $response->getReasonPhrase());
        // an empty phrase falls back to the default of the code
        $response->setStatus(404, '');
        $this->assertSame('Not Found', $response->getReasonPhrase());
        foreach (["\x00", "\x0b", "\x7f"] as $control) {
            try {
                $response->setStatus(200, "O{$control}K");
                $this->fail('A control character other than HTAB must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
        foreach ([0, 99, 600, 1000] as $code) {
            try {
                $response->setStatus($code);
                $this->fail('A status code outside 100..599 must be rejected');
            } catch (InvalidArgumentException) {
            }
        }
    }

    public function testParserDerivedReasonPhraseIsReplacedNotRejected(): void
    {
        /* a peer controls this text, so an invalid octet must not fail the
         * whole response: RFC 9110 5.5 lets a recipient replace it */
        $entity = new ResponseEntity();
        $entity->statusCode = 200;
        $entity->reasonPhrase = "O\0K";
        $entity->protocolVersion = '1.1';
        $response = Psr7::createResponseFromEntity($entity);
        $this->assertSame('O K', $response->getReasonPhrase());
    }

    public function testParserDerivedStatusCodeIsReplacedNotRejected(): void
    {
        /* the parser accepts a status code outside 100..599, so a peer could make
         * the client throw if the code were validated on this path */
        foreach ([0, 99, 600, 999] as $code) {
            $entity = new ResponseEntity();
            $entity->statusCode = $code;
            $entity->reasonPhrase = 'Weird';
            $entity->protocolVersion = '1.1';
            $response = Psr7::createResponseFromEntity($entity);
            $this->assertSame(502, $response->getStatusCode(), "code {$code} must be replaced");
        }
        // a code inside the range is kept as it is
        $entity = new ResponseEntity();
        $entity->statusCode = 599;
        $entity->reasonPhrase = 'Weird';
        $entity->protocolVersion = '1.1';
        $this->assertSame(599, Psr7::createResponseFromEntity($entity)->getStatusCode());
    }
}
