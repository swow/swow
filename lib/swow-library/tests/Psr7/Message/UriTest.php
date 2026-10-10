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
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Message\Uri;

/** @internal */
#[CoversClass(Uri::class)]
final class UriTest extends TestCase
{
    public function testComponentsAndEncoding(): void
    {
        $uri = new Uri('HTTPS://user:secret@EXAMPLE.COM:8443/a b/%20/%zz?q=a b#part one');
        $this->assertSame('https', $uri->getScheme());
        $this->assertSame('user:secret', $uri->getUserInfo());
        $this->assertSame('example.com', $uri->getHost());
        $this->assertSame(8443, $uri->getPort());
        $this->assertSame('user:secret@example.com:8443', $uri->getAuthority());
        $this->assertSame('/a%20b/%20/%25zz', $uri->getPath());
        $this->assertSame('q=a%20b', $uri->getQuery());
        $this->assertSame('part%20one', $uri->getFragment());
        $this->assertSame('https://user:secret@example.com:8443/a%20b/%20/%25zz?q=a%20b#part%20one', (string) $uri);
    }

    #[DataProvider('provideSerializationCases')]
    public function testSerialization(string $input, string $expected): void
    {
        $this->assertSame($expected, (new Uri($input))->toString());
    }

    /** @return iterable<string, array{string, string}> */
    public static function provideSerializationCases(): iterable
    {
        yield 'empty' => ['', ''];
        yield 'relative' => ['docs/readme?lang=en#usage', 'docs/readme?lang=en#usage'];
        yield 'network path' => ['//EXAMPLE.COM/path', '//example.com/path'];
        yield 'http default port' => ['http://example.com:80', 'http://example.com'];
        yield 'https default port' => ['https://example.com:443', 'https://example.com'];
        yield 'ipv6' => ['http://[::1]:8080/path', 'http://[::1]:8080/path'];
        yield 'userinfo without host' => ['mailto:user@example.com', 'mailto:user@example.com'];
    }

    public function testMutableSettersAndImmutableCopies(): void
    {
        $uri = new Uri('http://example.com/original?a=1#old');
        $this->assertSame($uri, $uri->withScheme('HTTP'));
        $this->assertSame($uri, $uri->withHost('EXAMPLE.COM'));
        $this->assertSame($uri, $uri->withPort(80));
        $this->assertSame($uri, $uri->withUserInfo(''));
        $this->assertSame($uri, $uri->withPath('/original'));
        $this->assertSame($uri, $uri->withQuery('a=1'));
        $this->assertSame($uri, $uri->withFragment('old'));

        $copy = $uri->withScheme('HTTPS')->withHost('OTHER.EXAMPLE')
            ->withPort(8443)->withUserInfo('reader', 'password')->withPath('next page')
            ->withQuery('q=%wrong')->withFragment('new fragment');
        $this->assertSame('http://example.com/original?a=1#old', $uri->toString());
        $this->assertSame('https://reader:password@other.example:8443/next%20page?q=%25wrong#new%20fragment', $copy->toString());

        $this->assertSame($uri, $uri->setScheme('HTTPS')->setHost('OTHER.EXAMPLE')
            ->setPort(443)->setUserInfo('reader')->setPath('/next')->setQuery('b=2')->setFragment('new'));
        $this->assertSame('https://reader@other.example/next?b=2#new', $uri->toString());
        $this->assertSame('', (new Uri())->setUserInfo('reader')->getAuthority());
    }

    public function testQueryParametersFollowQueryChanges(): void
    {
        $uri = new Uri('/?a=one&b=two');
        $this->assertSame(['a' => 'one', 'b' => 'two'], $uri->getQueryParams());
        $copy = $uri->withQuery('c=three');
        $this->assertSame(['c' => 'three'], $copy->getQueryParams());
        $this->assertSame(['a' => 'one', 'b' => 'two'], $uri->getQueryParams());
        $uri->setQuery('');
        $this->assertSame([], $uri->getQueryParams());
        $uri->setQueryParams(['name' => 'a b']);
        $this->assertSame('name=a+b', $uri->getQuery());
        $this->assertSame(['name' => 'a b'], $uri->getQueryParams());
        $copy = $uri->withQueryParams(['page' => '2']);
        $this->assertSame(['page' => '2'], $copy->getQueryParams());
        $this->assertSame(['name' => 'a b'], $uri->getQueryParams());
    }

    #[DataProvider('provideInvalidPortCases')]
    public function testInvalidPort(int $port): void
    {
        $this->expectException(InvalidArgumentException::class);
        (new Uri())->withPort($port);
    }

    /** @return iterable<string, array{int}> */
    public static function provideInvalidPortCases(): iterable
    {
        yield 'negative' => [-1];
        yield 'overflow' => [65536];
    }

    public function testInvalidUri(): void
    {
        $this->expectException(InvalidArgumentException::class);
        new Uri('http://example.com:invalid');
    }

    public function testApplyPartsReplacesComponents(): void
    {
        $uri = new Uri('https://user:secret@example.com:8443/path?q=1#fragment');
        $this->assertSame($uri, $uri->applyParts(['path' => '/relative']));
        $this->assertSame('/relative', $uri->toString());
        $this->assertSame($uri, $uri->apply('https://next.example/'));
        $this->assertSame('https://next.example/', $uri->toString());
    }

    public function testApplyReplacesPreviouslyParsedQuery(): void
    {
        $uri = new Uri('/?old=value');
        $this->assertSame(['old' => 'value'], $uri->getQueryParams());
        $uri->apply('/?new=value');
        $this->assertSame(['new' => 'value'], $uri->getQueryParams());
        $uri->applyParts(['path' => '/without-query']);
        $this->assertSame([], $uri->getQueryParams());
    }
}
