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
use RuntimeException;
use Swow\Object\ArrayableInterface;
use TypeError;
use ValueError;

use function error_get_last;
use function fopen;
use function is_resource;
use function is_string;
use function pathinfo;
use function sprintf;
use function Swow\Debug\isStringable;

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

/**
 * PSR-7 UploadedFile implementation (https://github.com/Nyholm/psr7/blob/master/src/UploadedFile.php)
 */
class UploadedFile implements UploadedFilePlusInterface, ArrayableInterface
{
    protected const ERRORS = [
        UPLOAD_ERR_OK => true,
        UPLOAD_ERR_INI_SIZE => true,
        UPLOAD_ERR_FORM_SIZE => true,
        UPLOAD_ERR_PARTIAL => true,
        UPLOAD_ERR_NO_FILE => true,
        UPLOAD_ERR_NO_TMP_DIR => true,
        UPLOAD_ERR_CANT_WRITE => true,
        UPLOAD_ERR_EXTENSION => true,
    ];

    protected StreamInterface $stream;

    protected ?int $size;

    protected int $error;

    protected ?string $clientFilename;

    protected ?string $clientMediaType;

    protected ?string $file = null;

    protected bool $moved = false;

    /**
     * @param string|resource|StreamInterface $streamOrFile
     */
    public function __construct(mixed $streamOrFile, ?int $size = null, int $error = UPLOAD_ERR_OK, ?string $clientFilename = null, ?string $clientMediaType = null)
    {
        if (!isset(static::ERRORS[$error])) {
            throw new ValueError(sprintf('Invalid error status for UploadedFile: %s', $error));
        }

        $this->error = $error;
        $this->size = $size;
        $this->clientFilename = $clientFilename;
        $this->clientMediaType = $clientMediaType;

        if ($this->error !== UPLOAD_ERR_OK) {
            return;
        }

        while (true) {
            if ($streamOrFile instanceof StreamInterface) {
                $this->stream = $streamOrFile;
                $metaData = $streamOrFile->getMetadata();
                if ($metaData['wrapper_type'] === 'plainfile') {
                    $this->file = $metaData['uri'];
                }
            } elseif (is_resource($streamOrFile)) {
                $streamOrFile = new PhpStream($streamOrFile);
                continue;
            } elseif (is_string($streamOrFile)) {
                $fp = @fopen($streamOrFile, 'rb');
                if ($fp === false) {
                    throw new RuntimeException(sprintf('Uploaded file "%s" could not be opened: %s', $streamOrFile, error_get_last()['message'] ?? 'unknown error'));
                }
                $this->stream = new PhpStream($fp);
                $this->file = $streamOrFile;
            }
            break;
        }
    }

    /**
     * @throws RuntimeException if is moved or not ok
     */
    protected function validateActive(): void
    {
        if ($this->error !== UPLOAD_ERR_OK) {
            throw new RuntimeException('Cannot retrieve stream due to upload error');
        }

        if ($this->moved) {
            throw new RuntimeException('Cannot retrieve stream after it has already been moved');
        }
    }

    public function getStream(): StreamInterface
    {
        $this->validateActive();

        return $this->stream;
    }

    public function moveTo(mixed $targetPath): void
    {
        if (!isStringable($targetPath)) {
            throw new TypeError(sprintf('%s(): Argument#1 ($targetPath) must be a string, %s given', __METHOD__, get_debug_type($targetPath)));
        }
        $targetPath = (string) $targetPath;
        if ($targetPath === '') {
            throw new ValueError(sprintf('%s(): Argument#1 ($targetPath) must be a non-empty string', __METHOD__));
        }

        $this->validateActive();

        if ($this->file !== null) {
            $this->moved = 'cli' === PHP_SAPI ? @rename($this->file, $targetPath) : @move_uploaded_file($this->file, $targetPath);
            if ($this->moved === false) {
                throw new RuntimeException(sprintf('Uploaded file could not be moved to "%s": %s', $targetPath, error_get_last()['message'] ?? 'unknown error'));
            }
        } else {
            $stream = $this->getStream();
            if ($stream->isSeekable()) {
                $stream->rewind();
            }
            if (($resource = @fopen($targetPath, 'wb+')) === false) {
                throw new RuntimeException(sprintf('The file "%s" cannot be opened: %s', $targetPath, error_get_last()['message'] ?? 'unknown error'));
            }
            $dest = new PhpStream($resource);
            while (!$stream->eof()) {
                if (!$dest->write($stream->read(1024 * 1024))) {
                    break;
                }
            }
            $this->moved = true;
        }
    }

    public function getSize(): ?int
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

    public function isMoved(): bool
    {
        return $this->moved;
    }

    /** @return array{'name': string|null, 'type': string|null, 'tmp_file': string|null, 'error': int, 'size': int|false} */
    public function toArray(): array
    {
        return [
            'name' => $this->getClientFilename(),
            'type' => $this->getClientMediaType(),
            'tmp_file' => $this->file,
            'error' => $this->getError(),
            'size' => $this->getSize(),
        ];
    }
}
