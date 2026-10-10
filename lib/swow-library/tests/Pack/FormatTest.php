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

namespace Swow\Tests\Pack;

use InvalidArgumentException;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Pack\Format;

use function strlen;

/** @internal */
#[CoversClass(Format::class)]
final class FormatTest extends TestCase
{
    #[DataProvider('provideSizeMatchesWireRepresentationCases')]
    public function testSizeMatchesWireRepresentation(string $format, int $size): void
    {
        $this->assertSame($size, Format::getSize($format));
        $this->assertSame($size, strlen(pack($format, 1)));
        $this->assertSame(1, unpack($format, pack($format, 1))[1]);
    }

    /** @return iterable<string, array{string, int}> */
    public static function provideSizeMatchesWireRepresentationCases(): iterable
    {
        foreach ([Format::INT8, Format::UINT8] as $format) {
            yield $format => [$format, 1];
        }
        foreach ([Format::INT16, Format::UINT16, Format::UINT16_BE, Format::UINT16_LE] as $format) {
            yield $format => [$format, 2];
        }
        foreach ([Format::INT32, Format::UINT32, Format::UINT32_BE, Format::UINT32_LE] as $format) {
            yield $format => [$format, 4];
        }
        foreach ([Format::INT64, Format::UINT64, Format::UINT64_BE, Format::UINT64_LE] as $format) {
            yield $format => [$format, 8];
        }
    }

    public function testUnknownFormat(): void
    {
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage("Unknown format type 'invalid'");
        Format::getSize('invalid');
    }
}
