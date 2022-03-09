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

namespace Swow\Stream;

use InvalidArgumentException;
use Psr\Http\Message\StreamInterface;
use RuntimeException;
use Stringable;
use function clearstatcache;
use function error_get_last;
use function fclose;
use function feof;
use function fopen;
use function fread;
use function fseek;
use function fstat;
use function ftell;
use function fwrite;
use function is_resource;
use function is_string;
use function stream_get_contents;
use function stream_get_meta_data;
use function var_export;
use const SEEK_CUR;
use const SEEK_SET;

/**
 * Refactored from https://github.com/Nyholm/psr7/blob/master/src/Stream.php
 */
class Psr7Stream implements StreamInterface, Stringable
{
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

    protected mixed $uri;

    protected ?int $size = null;

    protected bool $detached = false;

    protected function __construct()
    {
        // should be created via "static create()"
    }

    /**
     * Creates a new PSR-7 stream.
     * @param string|resource|StreamInterface $body
     */
    public static function create(mixed $body = ''): StreamInterface
    {
        if ($body instanceof StreamInterface) {
            return $body;
        }

        if (is_string($body)) {
            $resource = fopen('php://temp', 'rwb+');
            fwrite($resource, $body);
            $body = $resource;
        }

        if (is_resource($body)) {
            /* @phpstan-ignore-next-line */
            $new = new static();
            $new->stream = $body;
            $meta = stream_get_meta_data($new->stream);
            $new->seekable = $meta['seekable'] && fseek($new->stream, 0, SEEK_CUR) === 0;
            $new->readable = isset(static::READ_WRITE_HASH['read'][$meta['mode']]);
            $new->writable = isset(static::READ_WRITE_HASH['write'][$meta['mode']]);

            return $new;
        }

        throw new InvalidArgumentException('First argument to Stream::create() must be a string, resource or StreamInterface.');
    }

    protected function getUri(): mixed
    {
        if ($this->uri !== false) {
            $this->uri = $this->getMetadata('uri') ?? false;
        }

        return $this->uri;
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
            throw new RuntimeException('Unable to determine stream position: ' . (error_get_last()['message'] ?? ''));
        }

        return $result;
    }

    public function eof(): bool
    {
        return $this->detached || feof($this->stream);
    }

    public function isSeekable(): bool|null
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

    public function isWritable(): bool|null
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
            throw new RuntimeException('Unable to write to stream: ' . (error_get_last()['message'] ?? ''));
        }

        return $result;
    }

    public function isReadable(): bool|null
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
            throw new RuntimeException('Unable to read from stream: ' . (error_get_last()['message'] ?? ''));
        }

        return $result;
    }

    public function getContents(): string
    {
        if ($this->detached) {
            throw new RuntimeException('Stream is detached');
        }

        if (($contents = @stream_get_contents($this->stream)) === false) {
            throw new RuntimeException('Unable to read stream contents: ' . (error_get_last()['message'] ?? ''));
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

    public function detach()
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

    public function __toString(): string
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
