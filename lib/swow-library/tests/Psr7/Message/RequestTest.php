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

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Message\Request;
use Swow\Psr7\Psr7;

/**
 * @internal
 */
#[CoversClass(Request::class)]
#[CoversClass(Psr7::class)]
final class RequestTest extends TestCase
{
    public function testGetRequestTarget(): void
    {
        $request = Psr7::createRequest('GET', '/request');
        $this->assertSame('/request', $request->getRequestTarget());

        $request = Psr7::createRequest('POST', Psr7::createUriFromString('/test'));
        $this->assertSame('POST', $request->getMethod());
        $this->assertSame('/test', $request->getRequestTarget());
    }

    public function testDefaultUriAndMethodChanges(): void
    {
        $request = new Request();
        $this->assertSame('/', (string) $request->getUri());
        $this->assertSame('/', $request->getRequestTarget());
        $this->assertSame($request->getUri(), $request->getUri());
        $this->assertSame($request, $request->withMethod('GET'));
        $changed = $request->withMethod('POST');
        $this->assertNotSame($request, $changed);
        $this->assertSame('GET', $request->getMethod());
        $this->assertSame('POST', $changed->getMethod());
        $this->assertSame($request, $request->setMethod('PUT'));
        $this->assertSame('PUT', $request->getMethod());
    }

    #[DataProvider('provideRequestTargetUsesThePathAndQueryCases')]
    public function testRequestTargetUsesThePathAndQuery(string $uri, string $target): void
    {
        $request = Psr7::createRequest('GET', $uri);
        $this->assertSame($target, $request->getRequestTarget());
        $changed = $request->withRequestTarget('*');
        $this->assertSame('*', $changed->getRequestTarget());
        $this->assertSame($target, $request->getRequestTarget());
        $this->assertSame($request, $request->setRequestTarget('/custom'));
        $this->assertSame('/custom', $request->getRequestTarget());
    }

    /** @return iterable<string, array{string, string}> */
    public static function provideRequestTargetUsesThePathAndQueryCases(): iterable
    {
        yield 'empty path' => ['http://example.test?query=value', '/?query=value'];
        yield 'fragment is excluded' => ['http://example.test/path?query=value#fragment', '/path?query=value'];
        yield 'relative target' => ['/encoded%20path?value=a%2Bb', '/encoded%20path?value=a%2Bb'];
    }

    public function testUriChangesPreserveOrReplaceTheHostHeader(): void
    {
        $request = Psr7::createRequest('GET', 'http://first.test:8080/path');
        $this->assertSame('first.test:8080', $request->getHeaderLine('Host'));
        $uri = Psr7::createUriFromString('https://second.test/next');
        $preserved = $request->withUri($uri, true);
        $this->assertSame('first.test:8080', $preserved->getHeaderLine('Host'));
        $this->assertSame($uri, $preserved->getUri());
        $this->assertSame('http://first.test:8080/path', (string) $request->getUri());
        $replaced = $request->withUri($uri, false);
        $this->assertSame('second.test', $replaced->getHeaderLine('Host'));
        $this->assertSame($replaced, $replaced->withUri($uri));
        $relative = $replaced->withUri('/relative');
        $this->assertSame('second.test', $relative->getHeaderLine('Host'));
        $withoutHost = $request->withoutHeader('Host')->withUri($uri, true);
        $this->assertSame('second.test', $withoutHost->getHeaderLine('Host'));
    }

    public function testStandardHeadersDeriveTheMissingHostWithoutChangingTheRequest(): void
    {
        $request = Psr7::createRequest('GET', 'http://[::1]:8080/path', ['X-First' => 'value'])->withoutHeader('Host');
        $headers = $request->getStandardHeaders();
        $this->assertSame('Host', array_key_first($headers));
        $this->assertSame(['[::1]:8080'], $headers['Host']);
        $this->assertSame(['value'], $headers['X-First']);
        $this->assertFalse($request->hasHeader('Host'));
    }
}
