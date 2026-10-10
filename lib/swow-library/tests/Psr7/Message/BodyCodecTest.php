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
use JsonException;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Http\Mime\MimeType;
use Swow\Psr7\Message\BodyDecoder;
use Swow\Psr7\Message\BodyEncoder;
use Swow\Psr7\Psr7;

/** @internal */
#[CoversClass(BodyEncoder::class)]
#[CoversClass(BodyDecoder::class)]
final class BodyCodecTest extends TestCase
{
    #[DataProvider('provideRoundTripCases')]
    public function testRoundTrip(array $data, string $contentType): void
    {
        $encoded = BodyEncoder::encode($data, $contentType);
        $stream = Psr7::createStream($encoded);
        $this->assertSame($data, BodyDecoder::decode($stream, $contentType));
    }

    /** @return iterable<string, array{array<string, string>, string}> */
    public static function provideRoundTripCases(): iterable
    {
        yield 'json' => [['name' => 'a b', 'quote' => '"quoted"'], MimeType::JSON];
        yield 'form' => [['name' => 'a b', 'quote' => '"quoted"'], MimeType::X_WWW_FORM_URLENCODED];
        yield 'xml' => [['name' => 'reader', 'role' => 'writer'], MimeType::XML];
        yield 'xml text escaping' => [['name' => 'a & b < c', 'quoted' => '"quoted"'], MimeType::XML];
        yield 'default form' => [['name' => 'a b'], ''];
        yield 'empty json' => [[], MimeType::JSON];
    }

    public function testQueryAndJsonEncoding(): void
    {
        $this->assertSame('name=a+b&count=2', BodyEncoder::encodeQuery(['name' => 'a b', 'count' => 2]));
        $this->assertSame('{"flag":true,"count":2}', BodyEncoder::encodeJson(['flag' => true, 'count' => 2]));
        $this->assertSame([], BodyDecoder::decode(Psr7::createStream('')));
        $this->assertSame([], BodyDecoder::decode(Psr7::createStream('null'), MimeType::JSON));
        $this->assertSame([], BodyDecoder::decode(Psr7::createStream('42'), MimeType::JSON));
    }

    public function testInvalidJson(): void
    {
        $this->expectException(InvalidArgumentException::class);
        BodyDecoder::decode(Psr7::createStream('{'), MimeType::JSON);
    }

    public function testJsonEncodingRejectsInvalidUtf8(): void
    {
        $this->expectException(JsonException::class);
        BodyEncoder::encodeJson(['value' => "\xff"]);
    }
}
