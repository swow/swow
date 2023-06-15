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

namespace Swow\Psr7\Message;

use Psr\Http\Message\StreamInterface;
use Psr\Http\Message\UploadedFileInterface;

interface UploadedFilePlusInterface extends UploadedFileInterface
{
    public function getStream(): StreamInterface;

    /** @param string $targetPath */
    public function moveTo(mixed $targetPath): void;

    public function getSize(): ?int;

    public function getError(): int;

    public function getClientFilename(): ?string;

    public function getClientMediaType(): ?string;

    /** @return array{'name': string|null, 'type': string|null, 'tmp_file': string|null, 'error': int, 'size': int|false} */
    public function toArray(): array;
}
