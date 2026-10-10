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
use PHPUnit\Framework\TestCase;
use RuntimeException;
use Swow\Psr7\Message\UploadedFile;
use Swow\Psr7\Psr7;
use ValueError;

use function file_get_contents;
use function Swow\TestUtils\getRandomBytes;
use function sys_get_temp_dir;
use function tempnam;

use const UPLOAD_ERR_OK;
use const UPLOAD_ERR_PARTIAL;

/**
 * @internal
 */
#[CoversClass(UploadedFile::class)]
final class UploadedFileTest extends TestCase
{
    public function testBufferStreamWithoutWrapperMetadata(): void
    {
        $stream = Psr7::createStream('upload data');
        $uploadedFile = new UploadedFile($stream, 11, clientFilename: 'document.txt', clientMediaType: 'text/plain');

        $this->assertSame($stream, $uploadedFile->getStream());
        $this->assertSame('upload data', (string) $uploadedFile->getStream());
        $this->assertSame('txt', $uploadedFile->getExtension());
        $this->assertSame([
            'name' => 'document.txt',
            'type' => 'text/plain',
            'tmp_file' => null,
            'error' => UPLOAD_ERR_OK,
            'size' => 11,
        ], $uploadedFile->toArray());
    }

    public function testMove(): void
    {
        $tmpFile = tempnam(sys_get_temp_dir(), 'swow_uploaded_tmp_file');
        $targetPath = tempnam(sys_get_temp_dir(), 'swow_uploaded_file');
        $stream = Psr7::getDefaultStreamFactory()
            ->createStreamFromFile($tmpFile, 'w+');
        $random = getRandomBytes();
        $stream->write($random);
        $this->assertFileExists($tmpFile);
        $uploadedFile = Psr7::getDefaultUploadedFileFactory()
            ->createUploadedFile(stream: $stream, size: 0, clientFilename: 'filename.txt', clientMediaType: 'text/plain');
        $uploadedFile->moveTo($targetPath);
        $this->assertFileDoesNotExist($tmpFile);
        $this->assertFileExists($targetPath);
        $this->assertSame($random, @file_get_contents($targetPath));
    }

    public function testStreamUploadMovesCompleteContentAndBecomesInactive(): void
    {
        $targetPath = tempnam(sys_get_temp_dir(), 'swow_stream_upload');
        $stream = Psr7::createStream('complete contents');
        $stream->read(3);
        $uploadedFile = new UploadedFile($stream, 17);
        try {
            $this->assertFalse($uploadedFile->isMoved());
            $uploadedFile->moveTo($targetPath);
            $this->assertTrue($uploadedFile->isMoved());
            $this->assertSame('complete contents', file_get_contents($targetPath));
            $this->expectException(RuntimeException::class);
            $this->expectExceptionMessage('Cannot retrieve stream after it has already been moved');
            $uploadedFile->getStream();
        } finally {
            unlink($targetPath);
            $stream->close();
        }
    }

    public function testUploadErrorKeepsStreamUnavailable(): void
    {
        $uploadedFile = new UploadedFile('', error: UPLOAD_ERR_PARTIAL);
        $this->assertSame(UPLOAD_ERR_PARTIAL, $uploadedFile->getError());
        $this->assertNull($uploadedFile->getSize());
        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('Cannot retrieve stream due to upload error');
        $uploadedFile->getStream();
    }

    public function testInvalidUploadError(): void
    {
        $this->expectException(ValueError::class);
        new UploadedFile('', error: -1);
    }

    public function testEmptyTargetPath(): void
    {
        $file = new UploadedFile(Psr7::createStream('content'));
        $this->expectException(ValueError::class);
        $file->moveTo('');
    }

    public function testMissingClientFilename(): void
    {
        $file = new UploadedFile(Psr7::createStream('content'));
        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('Upload file name is not available');
        $file->getExtension();
    }

    public function testDestructor(): void
    {
        $tmpFile = tempnam(sys_get_temp_dir(), 'swow_uploaded_tmp_file');
        $stream = Psr7::getDefaultStreamFactory()
            ->createStreamFromFile($tmpFile, 'w+');
        $random = getRandomBytes();
        $stream->write($random);
        $uploadedFile = Psr7::getDefaultUploadedFileFactory()
            ->createUploadedFile(stream: $stream, size: 0, clientFilename: 'filename.txt', clientMediaType: 'text/plain');
        $this->assertFileExists($tmpFile);
        unset($uploadedFile);
        $this->assertFileDoesNotExist($tmpFile);
    }
}
