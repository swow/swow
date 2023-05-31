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

use PHPUnit\Framework\TestCase;
use Swow\Psr7\Psr7;

use function file_get_contents;
use function Swow\TestUtils\getRandomBytes;
use function sys_get_temp_dir;
use function tempnam;

/**
 * @internal
 * @covers \Swow\Psr7\Message\UploadedFile
 */
final class UploadedFileTest extends TestCase
{
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
