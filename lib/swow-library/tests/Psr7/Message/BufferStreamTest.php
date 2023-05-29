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
use PHPUnit\Framework\TestCase;
use Stringable;
use Swow\Psr7\Message\BufferStream;
use TypeError;
use ValueError;

use function str_repeat;

use const SEEK_CUR;
use const SEEK_END;

/**
 * @internal
 * @covers \Swow\Psr7\Message\BufferStream
 * @covers \Swow\Psr7\Psr7
 */
final class BufferStreamTest extends TestCase
{
    public function testConstructorWithString(): void
    {
        $stream = new BufferStream('test string');
        $this->assertSame('test string', $stream->getContents());
    }

    public function testConstructorWithStringable(): void
    {
        $obj = new class() implements Stringable {
            public function __toString(): string
            {
                return 'stringable object';
            }
        };
        $stream = new BufferStream($obj);
        $this->assertSame('stringable object', $stream->getContents());
    }

    public function testConstructorWithInvalidArgument(): void
    {
        $this->expectException(TypeError::class);
        new BufferStream([]);
    }

    public function testClose(): void
    {
        $stream = new BufferStream('test');
        $this->assertSame(4, $stream->getSize());
        $stream->close();
        $this->assertSame(0, $stream->getSize());
    }

    public function testGetSize(): void
    {
        $stream = new BufferStream('');
        $this->assertSame(0, $stream->getSize());
        $stream = new BufferStream('test');
        $this->assertSame(4, $stream->getSize());
        $this->assertSame(2, $stream->write('fa'));
        $this->assertSame(4, $stream->getSize());
        $stream->seek(0, SEEK_END);
        $this->assertSame(3, $stream->write('foo'));
        $this->assertSame(7, $stream->getSize());
    }

    public function testTell(): void
    {
        $stream = new BufferStream('test');
        $this->assertSame(0, $stream->tell());
        $this->assertSame('te', $stream->read(2));
        $this->assertSame(2, $stream->tell());
        $this->assertSame('st', $stream->read(2));
        $this->assertSame(4, $stream->tell());
    }

    public function testEof(): void
    {
        $stream = new BufferStream('');
        $this->assertTrue($stream->eof());
        $stream = new BufferStream('test');
        $this->assertFalse($stream->eof());
        $stream->read(4);
        $this->assertTrue($stream->eof());
    }

    public function testSeek(): void
    {
        $stream = new BufferStream('test');
        $stream->seek(2);
        $this->assertSame(2, $stream->tell());
        $stream->seek(-1, SEEK_END);
        $this->assertSame(3, $stream->tell());
        $stream->seek(1, SEEK_CUR);
        $this->assertSame(4, $stream->tell());
        $stream->seek(-2, SEEK_CUR);
        $this->assertSame(2, $stream->tell());
    }

    public function testSeekOverflow(): void
    {
        $stream = new BufferStream('test');
        $this->expectException(InvalidArgumentException::class);
        $stream->seek(1, SEEK_END);
    }

    public function testSeekInvalidWhence(): void
    {
        $stream = new BufferStream('test');
        $this->expectException(ValueError::class);
        $stream->seek(1, 999);
    }

    public function testRewind(): void
    {
        $stream = new BufferStream('test');
        $this->assertSame('te', $stream->read(2));
        $this->assertSame(2, $stream->tell());
        $stream->rewind();
        $this->assertSame(0, $stream->tell());
    }

    public function testWrite(): void
    {
        $stream = new BufferStream('test');
        $stream->seek(1);
        $this->assertSame(2, $stream->write('xx'));
        $stream->rewind();
        $this->assertSame('txxt', $stream->getContents());
        $stream->seek(0, SEEK_END);
        $this->assertSame(16, $stream->write(str_repeat('x', 16)));
        $this->assertSame(20, $stream->getSize());
    }

    public function testRead(): void
    {
        $stream = new BufferStream('test');
        $this->assertSame('te', $stream->read(2));
        $this->assertSame('st', $stream->read(2));
        $this->assertSame('', $stream->read(1));
        $this->assertSame('', $stream->read(-1));
        $stream->rewind();
        $this->assertSame('test', $stream->read(-1));
        $stream->seek(2);
        $this->assertSame('st', $stream->read(-1));
    }

    public function testGetContents(): void
    {
        $stream = new BufferStream('test');
        $this->assertSame('test', $stream->getContents());
        $this->assertSame('', $stream->getContents());
        $stream->seek(2);
        $this->assertSame('st', $stream->getContents());
    }

    public function testGetMetadata(): void
    {
        $stream = new BufferStream('test');
        $this->assertIsArray($stream->getMetadata());
        $this->assertSame('test', $stream->getMetadata('value'));
        $this->assertSame(4, $stream->getMetadata('size'));
        $this->assertSame(4, $stream->getMetadata('length'));
        $this->assertNull($stream->getMetadata('non-existent-key'));
    }

    public function testToString(): void
    {
        $stream = new BufferStream('test');
        $this->assertSame('test', $stream->toString());
    }

    public function testMagicToString(): void
    {
        $stream = new BufferStream('test');
        $this->assertSame('test', (string) $stream);
    }
}
