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
use RuntimeException;
use Swow\Psr7\Message\PhpStream;
use TypeError;

use function is_resource;

use const SEEK_END;

/** @internal */
#[CoversClass(PhpStream::class)]
final class PhpStreamTest extends TestCase
{
    public function testReadWritePositionAndSize(): void
    {
        $stream = new PhpStream(fopen('php://temp', 'w+b'));
        try {
            $this->assertTrue($stream->isReadable());
            $this->assertTrue($stream->isWritable());
            $this->assertTrue($stream->isSeekable());
            $this->assertSame(0, $stream->getSize());
            $this->assertSame(6, $stream->write('abcdef'));
            $this->assertSame(6, $stream->getSize());
            $this->assertSame(6, $stream->getSize());
            $this->assertSame(6, $stream->tell());
            $stream->rewind();
            $this->assertSame('ab', $stream->read(2));
            $this->assertSame(2, $stream->tell());
            $this->assertSame('cdef', $stream->getContents());
            $this->assertTrue($stream->eof());
            $stream->seek(-2, SEEK_END);
            $this->assertSame('ef', $stream->read(2));
            $this->assertSame('abcdef', $stream->toString());
            $this->assertSame('php://temp', $stream->getMetadata('uri'));
            $this->assertArrayHasKey('mode', $stream->getMetadata());
            $this->assertNull($stream->getMetadata('missing'));
        } finally {
            $stream->close();
        }
    }

    public function testDetachTransfersOwnership(): void
    {
        $resource = fopen('php://temp', 'w+b');
        $stream = new PhpStream($resource);
        $stream->write('retained');
        $this->assertSame($resource, $stream->detach());
        try {
            $stream->close();
            $this->assertTrue(is_resource($resource));
            rewind($resource);
            $this->assertSame('retained', stream_get_contents($resource));
            $this->assertNull($stream->detach());
            $this->assertNull($stream->getSize());
            $this->assertTrue($stream->eof());
            $this->assertFalse($stream->isReadable());
            $this->assertFalse($stream->isWritable());
            $this->assertFalse($stream->isSeekable());
            $this->assertSame([], $stream->getMetadata());
            $this->assertNull($stream->getMetadata('uri'));
        } finally {
            fclose($resource);
        }
    }

    #[DataProvider('provideDetachedOperationsFailCases')]
    public function testDetachedOperationsFail(string $method, array $arguments): void
    {
        $stream = new PhpStream(fopen('php://temp', 'w+b'));
        $stream->close();
        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('Stream is detached');
        $stream->{$method}(...$arguments);
    }

    /** @return iterable<string, array{string, array<int|string>}> */
    public static function provideDetachedOperationsFailCases(): iterable
    {
        yield 'tell' => ['tell', []];
        yield 'seek' => ['seek', [0]];
        yield 'read' => ['read', [1]];
        yield 'write' => ['write', ['data']];
        yield 'getContents' => ['getContents', []];
    }

    public function testCloseReleasesResource(): void
    {
        $resource = fopen('php://temp', 'w+b');
        $stream = new PhpStream($resource);
        $stream->close();
        $stream->close();
        $this->assertFalse(is_resource($resource));
        $this->assertNull($stream->getSize());
    }

    public function testReadOnlyStreamRejectsWrites(): void
    {
        $stream = new PhpStream(fopen(__FILE__, 'rb'));
        try {
            $this->assertTrue($stream->isReadable());
            $this->assertFalse($stream->isWritable());
            $this->assertGreaterThan(0, $stream->getSize());
            $this->expectException(RuntimeException::class);
            $this->expectExceptionMessage('Cannot write to a non-writable stream');
            $stream->write('data');
        } finally {
            $stream->close();
        }
    }

    public function testInvalidResource(): void
    {
        $this->expectException(TypeError::class);
        new PhpStream('php://temp');
    }
}
