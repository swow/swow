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

namespace Swow\Tests\Http\Protocol;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Http\Protocol\H2Exception;
use Swow\Http\Protocol\H2Frame;

/**
 * @internal
 */
#[CoversClass(H2Frame::class)]
#[CoversClass(H2Exception::class)]
final class H2FrameTest extends TestCase
{
    public function testEncodeAndDecodeHeader(): void
    {
        $header = H2Frame::encodeHeader(5, H2Frame::TYPE_DATA, H2Frame::FLAG_END_STREAM, 1);
        $decoded = H2Frame::decodeHeader($header);

        $this->assertSame(5, $decoded['length']);
        $this->assertSame(H2Frame::TYPE_DATA, $decoded['type']);
        $this->assertSame(H2Frame::FLAG_END_STREAM, $decoded['flags']);
        $this->assertSame(1, $decoded['streamId']);
    }

    public function testFromHeaderAndPayloadCreatesFrame(): void
    {
        $payload = 'hello';
        $header = H2Frame::encodeHeader(strlen($payload), H2Frame::TYPE_DATA, 0, 3);
        $frame = H2Frame::fromHeaderAndPayload($header, $payload);

        $this->assertSame(H2Frame::TYPE_DATA, $frame->type);
        $this->assertSame(0, $frame->flags);
        $this->assertSame(3, $frame->streamId);
        $this->assertSame($payload, $frame->payload);
        $this->assertSame($header . $payload, $frame->encode());
    }

    public function testDecodeHeaderRejectsInvalidHeaderLength(): void
    {
        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Invalid frame header length');

        H2Frame::decodeHeader('short');
    }

    public function testFromHeaderAndPayloadRejectsMismatchedPayloadLength(): void
    {
        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('Frame payload length mismatch');

        $header = H2Frame::encodeHeader(4, H2Frame::TYPE_DATA, 0, 1);
        H2Frame::fromHeaderAndPayload($header, 'abc');
    }
}
