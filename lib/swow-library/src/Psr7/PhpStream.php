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

namespace Swow\Psr7;

use RuntimeException;
use Swow\Object\StringableTrait;
use TypeError;

use function clearstatcache;
use function error_get_last;
use function fclose;
use function feof;
use function fread;
use function fseek;
use function fstat;
use function ftell;
use function fwrite;
use function is_resource;
use function sprintf;
use function stream_get_contents;
use function stream_get_meta_data;
use function var_export;

use const SEEK_CUR;
use const SEEK_SET;

/**
 * Refactored from https://github.com/Nyholm/psr7/blob/master/src/Stream.php
 */
class PhpStream implements StreamPlusInterface
{
    use StringableTrait;

    /** @var array<string, array<string, bool>> Hash of readable and writable stream types */
    protected const READ_WRITE_HASH = [
        'read' => [
            'r' => true,
            'w+' => true,
            'r+' => true,
            'x+' => true,
            'c+' => true,
            'rb' => true,
            'w+b' => true,
            'r+b' => true,
            'x+b' => true,
            'c+b' => true,
            'rt' => true,
            'w+t' => true,
            'r+t' => true,
            'x+t' => true,
            'c+t' => true,
            'a+' => true,
        ],
        'write' => [
            'w' => true,
            'w+' => true,
            'rw' => true,
            'r+' => true,
            'x+' => true,
            'c+' => true,
            'wb' => true,
            'w+b' => true,
            'r+b' => true,
            'x+b' => true,
            'c+b' => true,
            'w+t' => true,
            'r+t' => true,
            'x+t' => true,
            'c+t' => true,
            'a' => true,
            'a+' => true,
        ],
    ];

    /** @psalm-var closed-resource|resource|null */
    /** @var resource|null A resource reference */
    protected $stream;

    protected bool $seekable = false;

    protected bool $readable = false;

    protected bool $writable = false;

    protected ?string $uri = null;

    protected ?int $size = null;

    protected bool $detached = false;

    /**
     * @param resource $resource php stream resource
     */
    public function __construct($resource)
    {
        if (!is_resource($resource)) {
            throw new TypeError(sprintf('%s(): Argument#1 ($stream) must be a resource, %s given', __METHOD__, get_debug_type($resource)));
        }

        $this->stream = $resource;
        $meta = stream_get_meta_data($resource);
        $this->seekable = $meta['seekable'] && fseek($this->stream, 0, SEEK_CUR) === 0;
        $this->readable = isset(static::READ_WRITE_HASH['read'][$meta['mode']]);
        $this->writable = isset(static::READ_WRITE_HASH['write'][$meta['mode']]);
    }

    protected function getUri(): ?string
    {
        return $this->uri ??= $this->getMetadata('uri');
    }

    public function getSize(): ?int
    {
        if ($this->size !== null) {
            return $this->size;
        }

        if ($this->detached) {
            return null;
        }

        // Clear the stat cache if the stream has a URI
        if ($uri = $this->getUri()) {
            clearstatcache(true, $uri);
        }

        $stats = fstat($this->stream);
        if (isset($stats['size'])) {
            $this->size = $stats['size'];

            return $this->size;
        }

        return null;
    }

    public function tell(): int
    {
        if ($this->detached) {
            throw new RuntimeException('Stream is detached');
        }

        if (($result = @ftell($this->stream)) === false) {
            throw new RuntimeException('Unable to determine stream position: ' . (error_get_last()['message'] ?? 'unknown error'));
        }

        return $result;
    }

    public function eof(): bool
    {
        return $this->detached || feof($this->stream);
    }

    public function isSeekable(): bool
    {
        return $this->seekable;
    }

    public function seek($offset, $whence = SEEK_SET): void
    {
        if ($this->detached) {
            throw new RuntimeException('Stream is detached');
        }

        if (!$this->seekable) {
            throw new RuntimeException('Stream is not seekable');
        }

        if (fseek($this->stream, $offset, $whence) === -1) {
            throw new RuntimeException('Unable to seek to stream position "' . $offset . '" with whence ' . var_export($whence, true));
        }
    }

    public function rewind(): void
    {
        $this->seek(0);
    }

    public function isWritable(): bool
    {
        return $this->writable;
    }

    public function write($string): int
    {
        if ($this->detached) {
            throw new RuntimeException('Stream is detached');
        }

        if (!$this->writable) {
            throw new RuntimeException('Cannot write to a non-writable stream');
        }

        // We can't know the size after writing anything
        $this->size = null;

        if (false === $result = @fwrite($this->stream, $string)) {
            throw new RuntimeException('Unable to write to stream: ' . (error_get_last()['message'] ?? 'unknown error'));
        }

        return $result;
    }

    public function isReadable(): bool
    {
        return $this->readable;
    }

    public function read($length): string
    {
        if ($this->detached) {
            throw new RuntimeException('Stream is detached');
        }

        if (!$this->readable) {
            throw new RuntimeException('Cannot read from non-readable stream');
        }

        if (($result = @fread($this->stream, $length)) === false) {
            throw new RuntimeException('Unable to read from stream: ' . (error_get_last()['message'] ?? 'unknown error'));
        }

        return $result;
    }

    public function getContents(): string
    {
        if ($this->detached) {
            throw new RuntimeException('Stream is detached');
        }

        if (($contents = @stream_get_contents($this->stream)) === false) {
            throw new RuntimeException('Unable to read stream contents: ' . (error_get_last()['message'] ?? 'unknown error'));
        }

        return $contents;
    }

    public function getMetadata(mixed $key = null): mixed
    {
        if ($this->detached) {
            return $key ? null : [];
        }

        $meta = stream_get_meta_data($this->stream);

        if ($key === null) {
            return $meta;
        }

        return $meta[$key] ?? null;
    }

    public function detach(): mixed
    {
        if ($this->detached) {
            return null;
        }

        $result = $this->stream;
        $this->stream = $this->size = $this->uri = null;
        $this->readable = $this->writable = $this->seekable = false;
        $this->detached = true;

        return $result;
    }

    public function close(): void
    {
        if (isset($this->stream)) {
            if (is_resource($this->stream)) {
                fclose($this->stream);
            }
            $this->detach();
        }
    }

    public function toString(): string
    {
        if ($this->isSeekable()) {
            $this->seek(0);
        }

        return $this->getContents();
    }

    public function __destruct()
    {
        $this->close();
    }
}
