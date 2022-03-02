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

namespace Swow\Http;

use InvalidArgumentException;
use Psr\Http\Message\StreamInterface;
use Psr\Http\Message\UploadedFileInterface;
use RuntimeException;
use SplFileInfo;
use Stringable;
use Swow\Stream\Psr7Stream;
use function fopen;
use function in_array;
use function is_string;
use function is_uploaded_file;
use function json_encode;
use function move_uploaded_file;
use function pathinfo;
use function rename;
use function sprintf;
use const JSON_THROW_ON_ERROR;
use const PATHINFO_EXTENSION;
use const PHP_SAPI;
use const UPLOAD_ERR_CANT_WRITE;
use const UPLOAD_ERR_EXTENSION;
use const UPLOAD_ERR_FORM_SIZE;
use const UPLOAD_ERR_INI_SIZE;
use const UPLOAD_ERR_NO_FILE;
use const UPLOAD_ERR_NO_TMP_DIR;
use const UPLOAD_ERR_OK;
use const UPLOAD_ERR_PARTIAL;

class UploadFile extends SplFileInfo implements UploadedFileInterface, Stringable
{
    /** @var array<int> */
    protected static array $errors = [
        UPLOAD_ERR_OK,
        UPLOAD_ERR_INI_SIZE,
        UPLOAD_ERR_FORM_SIZE,
        UPLOAD_ERR_PARTIAL,
        UPLOAD_ERR_NO_FILE,
        UPLOAD_ERR_NO_TMP_DIR,
        UPLOAD_ERR_CANT_WRITE,
        UPLOAD_ERR_EXTENSION,
    ];

    protected ?string $tmpFile = null;

    protected bool $moved = false;

    public function __construct(
        string $tmpFile,
        protected int $size,
        protected int $error,
        protected ?string $clientFilename = null,
        protected ?string $clientMediaType = null
    ) {
        if (!in_array($error, static::$errors, true)) {
            throw new InvalidArgumentException('Invalid error status for UploadedFile');
        }
        if ($this->isOk()) {
            $this->tmpFile = $tmpFile;
        }
        parent::__construct($tmpFile);
    }

    public function getStream(): StreamInterface
    {
        if ($this->moved) {
            throw new RuntimeException('Uploaded file is moved');
        }
        return Psr7Stream::create(fopen($this->tmpFile, 'rb+'));
    }

    public function moveTo($targetPath): void
    {
        $this->validateActive();

        if (!is_string($targetPath) || empty($targetPath)) {
            throw new InvalidArgumentException('Invalid path provided for move operation');
        }

        if ($this->tmpFile) {
            // FIXME: support move_uploaded_file()
            $this->moved = PHP_SAPI === 'cli' ? @rename($this->tmpFile, $targetPath) : @move_uploaded_file($this->tmpFile, $targetPath);
        }

        if (!$this->moved) {
            throw new RuntimeException(sprintf('Uploaded file could not be move to %s (%s)', $targetPath, error_get_last()['message']));
        }
    }

    public function getSize(): int
    {
        return $this->size;
    }

    public function getError(): int
    {
        return $this->error;
    }

    public function getClientFilename(): ?string
    {
        return $this->clientFilename;
    }

    public function getClientMediaType(): ?string
    {
        return $this->clientMediaType;
    }

    public function getExtension(): string
    {
        $clientFileName = $this->getClientFilename();
        if (!$clientFileName) {
            throw new RuntimeException('Upload file name is not available');
        }
        return pathinfo($clientFileName, PATHINFO_EXTENSION);
    }

    public function isValid(): bool
    {
        $isOk = $this->error === UPLOAD_ERR_OK;

        // FIXME: support is_uploaded_file() in cli mode
        return $isOk && (PHP_SAPI === 'cli' || is_uploaded_file($this->getPathname()));
    }

    public function isMoved(): bool
    {
        return $this->moved;
    }

    /** @return array{'name': null|string, 'type': null|string, 'tmp_file': null|string, 'error': int, 'size': int|false} */
    public function toArray(): array
    {
        return [
            'name' => $this->getClientFilename(),
            'type' => $this->getClientMediaType(),
            'tmp_file' => $this->tmpFile,
            'error' => $this->getError(),
            'size' => $this->getSize(),
        ];
    }

    public function __toString(): string
    {
        return (string) json_encode($this->toArray(), JSON_THROW_ON_ERROR);
    }

    /** @return array<string, mixed> */
    public function __debugInfo(): array
    {
        return $this->toArray() + [
            'is_valid' => $this->isValid(),
            'is_moved' => $this->isMoved(),
        ];
    }

    protected function isOk(): bool
    {
        return $this->error === UPLOAD_ERR_OK;
    }

    /**
     * @throws RuntimeException if is moved or not ok
     */
    protected function validateActive(): void
    {
        if ($this->isOk() === false) {
            throw new RuntimeException('Cannot retrieve stream due to upload error');
        }
        if ($this->isMoved()) {
            throw new RuntimeException('Cannot retrieve stream after it has already been moved');
        }
    }
}
