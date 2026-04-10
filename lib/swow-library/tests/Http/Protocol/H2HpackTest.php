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
use Swow\Http\Protocol\H2Hpack;
use Swow\Psr7\Server\H2ServerConnection;

/**
 * @internal
 */
#[CoversClass(H2Hpack::class)]
#[CoversClass(H2ServerConnection::class)]
final class H2HpackTest extends TestCase
{
    public function testDecodeRfcC4RequestWithHuffman(): void
    {
        $decoder = new H2Hpack();
        $headers = $decoder->decode(hex2bin('828684418cf1e3c2e5f23a6ba0ab90f4ff'));

        $this->assertCount(4, $headers);
        $this->assertSame(':method', $headers[0]['name']);
        $this->assertSame('GET', $headers[0]['value']);
        $this->assertSame(':scheme', $headers[1]['name']);
        $this->assertSame('http', $headers[1]['value']);
        $this->assertSame(':path', $headers[2]['name']);
        $this->assertSame('/', $headers[2]['value']);
        $this->assertSame(':authority', $headers[3]['name']);
        $this->assertSame('www.example.com', $headers[3]['value']);
    }

    public function testDecodeIndexedHeaders(): void
    {
        $decoder = new H2Hpack();
        $headers = $decoder->decode("\x82\x87\x84");

        $this->assertSame(':method', $headers[0]['name']);
        $this->assertSame('GET', $headers[0]['value']);
        $this->assertSame(':scheme', $headers[1]['name']);
        $this->assertSame('https', $headers[1]['value']);
        $this->assertSame(':path', $headers[2]['name']);
        $this->assertSame('/', $headers[2]['value']);
    }

    public function testDecodeLiteralHeaders(): void
    {
        $decoder = new H2Hpack();
        $headers = $decoder->decode(
            "\x82" .               // :method GET
            "\x41\x0bexample.com" . // literal with indexed name :authority
            "\x40\x04host\x0bexample.com" // literal with incremental indexing
        );

        $this->assertSame(':method', $headers[0]['name']);
        $this->assertSame('GET', $headers[0]['value']);
        $this->assertSame(':authority', $headers[1]['name']);
        $this->assertSame('example.com', $headers[1]['value']);
        $this->assertSame('host', $headers[2]['name']);
        $this->assertSame('example.com', $headers[2]['value']);
    }

    public function testDecodeSupportsDynamicTableAcrossBlocks(): void
    {
        $decoder = new H2Hpack();
        $decoder->decode(hex2bin('400a637573746f6d2d6b65790d637573746f6d2d686561646572'));
        $headers = $decoder->decode(hex2bin('be'));

        $this->assertCount(1, $headers);
        $this->assertSame('custom-key', $headers[0]['name']);
        $this->assertSame('custom-header', $headers[0]['value']);
    }

    public function testDecodeEvictsDynamicEntriesAfterTableShrink(): void
    {
        $decoder = new H2Hpack(64);
        $decoder->decode(hex2bin('3f01'));
        $decoder->decode(hex2bin('40036162630464656667'));

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('HPACK index out of range');

        $decoder->decode(hex2bin('be'));
    }

    public function testRecvRequestFromHeaderBlockSupportsDynamicTableAcrossCalls(): void
    {
        $connection = new H2ServerConnection();
        $connection->recvRequestFromHeaderBlock(
            hex2bin('828784010b6578616d706c652e636f6d400a637573746f6d2d6b65790c637573746f6d2d76616c7565')
        );
        $request = $connection->recvRequestFromHeaderBlock(
            hex2bin('828784010b6578616d706c652e636f6dbe')
        );

        $this->assertSame('custom-value', $request->getHeaderLine('custom-key'));
    }

    public function testDecodeRejectsInvalidIndex(): void
    {
        $decoder = new H2Hpack();

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('HPACK index must be positive');

        $decoder->decode("\x80");
    }

    public function testDecodeRejectsTableSizeUpdateBeyondLimit(): void
    {
        $decoder = new H2Hpack(128);

        $this->expectException(H2Exception::class);
        $this->expectExceptionMessage('HPACK table size update exceeds allowed maximum');

        $decoder->decode("\x3f\xe1\x01");
    }

    public function testRecvRequestFromHeaderBlockCreatesServerRequest(): void
    {
        $connection = new H2ServerConnection();
        $request = $connection->recvRequestFromHeaderBlock(
            "\x82" .                 // :method GET
            "\x87" .                 // :scheme https
            "\x41\x0bexample.com" .  // :authority: example.com
            "\x44\x06/hello"         // :path: /hello
        );

        $this->assertSame('GET', $request->getMethod());
        $this->assertSame('https://example.com/hello', (string) $request->getUri());
        $this->assertSame('example.com', $request->getHeaderLine('host'));
    }
}
