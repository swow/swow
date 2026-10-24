<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Gguf;

use Throwable;

use function array_key_exists;
use function fclose;
use function fread;
use function fseek;
use function ftell;
use function is_file;
use function is_int;
use function is_resource;
use function ord;
use function sprintf;
use function str_starts_with;
use function strlen;
use function unpack;

use const SEEK_SET;

final class GgufFile implements WeightStore
{
    private const MAGIC = 'GGUF';

    private const MAX_ENTRIES = 1_000_000;

    private const LAZY_ARRAY_THRESHOLD = 1024;

    /** @var resource|null */
    private $handle;

    private GgufMetadata $metadata;

    /** @var array<string, TensorInfo> */
    private array $tensors = [];

    private int $dataStart = 0;

    private int $alignment = 32;

    /**
     * @param resource $handle
     */
    private function __construct(
        private string $path,
        private int $fileSize,
        $handle,
        private int $version,
    ) {
        $this->handle = $handle;
    }

    public static function open(string $path): self
    {
        if (!is_file($path)) {
            throw new GgufException("GGUF file does not exist: {$path}");
        }
        $handle = fopen($path, 'rb');
        if ($handle === false) {
            throw new GgufException("Unable to open GGUF file: {$path}");
        }
        $size = filesize($path);
        if ($size === false) {
            fclose($handle);
            throw new GgufException("Unable to stat GGUF file: {$path}");
        }

        try {
            $file = new self($path, $size, $handle, 0);
            $file->parse();

            return $file;
        } catch (Throwable $e) {
            fclose($handle);
            throw $e;
        }
    }

    public function path(): string
    {
        return $this->path;
    }

    public function fileSize(): int
    {
        return $this->fileSize;
    }

    public function version(): int
    {
        return $this->version;
    }

    public function alignment(): int
    {
        return $this->alignment;
    }

    public function dataStart(): int
    {
        return $this->dataStart;
    }

    public function metadata(): GgufMetadata
    {
        return $this->metadata;
    }

    /** @return array<string, TensorInfo> */
    public function tensors(): array
    {
        return $this->tensors;
    }

    public function tensor(string $name): TensorInfo
    {
        if (!array_key_exists($name, $this->tensors)) {
            throw new GgufException("Unknown tensor: {$name}");
        }

        return $this->tensors[$name];
    }

    public function read(string $name, int $offset, int $length): string
    {
        if ($offset < 0 || $length < 0) {
            throw new GgufException('Negative tensor read offset or length');
        }
        $tensor = $this->tensor($name);
        if ($offset + $length > $tensor->byteSize()) {
            throw new GgufException(
                sprintf(
                    'Tensor read out of range: %s offset=%d length=%d size=%d',
                    $name,
                    $offset,
                    $length,
                    $tensor->byteSize(),
                ),
            );
        }
        $this->seekAbsolute($tensor->dataOffset() + $offset);

        return $this->readBytes($length);
    }

    public function row(string $name, int $row): string
    {
        $tensor = $this->tensor($name);
        if ($row < 0 || $row >= $tensor->rows()) {
            throw new GgufException("Row out of range for {$name}: {$row}");
        }

        return $this->read($name, $row * $tensor->rowBytes(), $tensor->rowBytes());
    }

    public function rows(string $name, int $rowBegin, int $rowEnd): string
    {
        $tensor = $this->tensor($name);
        if ($rowBegin < 0 || $rowEnd < $rowBegin || $rowEnd > $tensor->rows()) {
            throw new GgufException(
                "Row range out of range for {$name}: {$rowBegin}..{$rowEnd}",
            );
        }

        return $this->read(
            $name,
            $rowBegin * $tensor->rowBytes(),
            ($rowEnd - $rowBegin) * $tensor->rowBytes(),
        );
    }

    public function readAll(string $name): string
    {
        $tensor = $this->tensor($name);

        return $this->read($name, 0, $tensor->byteSize());
    }

    /** @return list<mixed> */
    public function readArray(GgufArrayRef $reference): array
    {
        $this->seekAbsolute($reference->offset());
        $values = [];
        for ($index = 0; $index < $reference->count(); $index++) {
            $values[] = $this->readValue($reference->elementType(), null, false);
        }

        return $values;
    }

    public function close(): void
    {
        if (is_resource($this->handle)) {
            fclose($this->handle);
        }
        $this->handle = null;
    }

    public function __destruct()
    {
        $this->close();
    }

    private function parse(): void
    {
        $magic = $this->readBytes(4);
        if ($magic !== self::MAGIC) {
            throw new GgufException('Not a GGUF file');
        }
        $this->version = $this->readUInt32();
        if ($this->version < 2 || $this->version > 3) {
            throw new GgufException("Unsupported GGUF version: {$this->version}");
        }

        $tensorCount = $this->readUInt64();
        $metadataCount = $this->readUInt64();
        if ($tensorCount > self::MAX_ENTRIES || $metadataCount > self::MAX_ENTRIES) {
            throw new GgufException('GGUF entry count is unreasonable');
        }

        $metadata = [];
        for ($index = 0; $index < $metadataCount; $index++) {
            $key = $this->readString();
            $type = $this->readUInt32();
            $metadata[$key] = $this->readValue($type, $key, true);
        }
        $this->metadata = new GgufMetadata($metadata);

        $alignment = $metadata['general.alignment'] ?? 32;
        if (!is_int($alignment) || $alignment <= 0 || ($alignment & ($alignment - 1)) !== 0) {
            throw new GgufException('Invalid general.alignment');
        }
        $this->alignment = $alignment;

        $rawTensors = [];
        for ($index = 0; $index < $tensorCount; $index++) {
            $name = $this->readString();
            if (isset($rawTensors[$name])) {
                throw new GgufException("Duplicate tensor name: {$name}");
            }
            $dimensionCount = $this->readUInt32();
            if ($dimensionCount < 1 || $dimensionCount > 8) {
                throw new GgufException("Invalid dimension count for tensor {$name}");
            }
            $dimensions = [];
            $elements = 1;
            for ($dimension = 0; $dimension < $dimensionCount; $dimension++) {
                $size = $this->readUInt64();
                if ($size <= 0) {
                    throw new GgufException("Invalid dimension for tensor {$name}");
                }
                $dimensions[] = $size;
                $elements *= $size;
            }
            $type = $this->readUInt32();
            GgmlType::assertSupported($type);
            $offset = $this->readUInt64();
            $rawTensors[$name] = [
                'name' => $name,
                'dimensions' => $dimensions,
                'type' => $type,
                'offset' => $offset,
                'byteSize' => GgmlType::byteSize($type, $elements),
            ];
        }

        $this->dataStart = self::align($this->position(), $this->alignment);
        if ($this->dataStart > $this->fileSize) {
            throw new GgufException('Tensor data starts beyond end of file');
        }

        foreach ($rawTensors as $raw) {
            $absolute = $this->dataStart + $raw['offset'];
            if ($absolute < $this->dataStart || $absolute + $raw['byteSize'] > $this->fileSize) {
                throw new GgufException("Tensor {$raw['name']} is outside the file");
            }
            $this->tensors[$raw['name']] = new TensorInfo(
                $raw['name'],
                $raw['dimensions'],
                $raw['type'],
                $raw['offset'],
                $absolute,
                $raw['byteSize'],
            );
        }
    }

    private function readValue(int $type, ?string $key, bool $lazy): mixed
    {
        return match ($type) {
            0 => ord($this->readBytes(1)),
            1 => $this->readInt8(),
            2 => unpack('v', $this->readBytes(2))[1],
            3 => $this->readInt16(),
            4 => $this->readUInt32(),
            5 => $this->readInt32(),
            6 => unpack('g', $this->readBytes(4))[1],
            7 => ord($this->readBytes(1)) !== 0,
            8 => $this->readString(),
            9 => $this->readMetadataArray($key, $lazy),
            10 => $this->readUInt64(),
            11 => $this->readInt64(),
            12 => unpack('e', $this->readBytes(8))[1],
            default => throw new GgufException("Unsupported metadata value type: {$type}"),
        };
    }

    /** @return list<mixed>|GgufArrayRef */
    private function readMetadataArray(?string $key, bool $lazy): array|GgufArrayRef
    {
        $elementType = $this->readUInt32();
        $count = $this->readUInt64();
        if ($lazy && ($count > self::LAZY_ARRAY_THRESHOLD || ($key !== null && str_starts_with($key, 'tokenizer.')))) {
            $offset = $this->position();
            $this->skipArray($elementType, $count);

            return new GgufArrayRef($this, $offset, $elementType, $count);
        }

        $values = [];
        for ($index = 0; $index < $count; $index++) {
            $values[] = $this->readValue($elementType, null, false);
        }

        return $values;
    }

    private function skipArray(int $elementType, int $count): void
    {
        if ($elementType === 8) {
            for ($index = 0; $index < $count; $index++) {
                $length = $this->readUInt64();
                $this->skip($length);
            }

            return;
        }
        if ($elementType === 9) {
            for ($index = 0; $index < $count; $index++) {
                $nestedType = $this->readUInt32();
                $nestedCount = $this->readUInt64();
                $this->skipArray($nestedType, $nestedCount);
            }

            return;
        }

        $this->skip($count * self::scalarSize($elementType));
    }

    private static function scalarSize(int $type): int
    {
        return match ($type) {
            0, 1, 7 => 1,
            2, 3 => 2,
            4, 5, 6 => 4,
            10, 11, 12 => 8,
            default => throw new GgufException("Unsupported array element type: {$type}"),
        };
    }

    private function readString(): string
    {
        $length = $this->readUInt64();
        if ($length > $this->fileSize || $length > 0x7FFFFFFF) {
            throw new GgufException('String length is unreasonable');
        }

        return $this->readBytes($length);
    }

    private function readUInt32(): int
    {
        return unpack('V', $this->readBytes(4))[1];
    }

    private function readInt32(): int
    {
        $value = $this->readUInt32();

        return $value >= 0x80000000 ? $value - 0x100000000 : $value;
    }

    private function readUInt64(): int
    {
        return unpack('P', $this->readBytes(8))[1];
    }

    private function readInt64(): int
    {
        return $this->readUInt64();
    }

    private function readInt8(): int
    {
        $value = ord($this->readBytes(1));

        return $value >= 0x80 ? $value - 0x100 : $value;
    }

    private function readInt16(): int
    {
        $value = unpack('v', $this->readBytes(2))[1];

        return $value >= 0x8000 ? $value - 0x10000 : $value;
    }

    private function readBytes(int $length): string
    {
        if ($length < 0) {
            throw new GgufException('Negative read length');
        }
        if ($length === 0) {
            return '';
        }
        $position = $this->position();
        if ($position + $length > $this->fileSize) {
            throw new GgufException('Unexpected end of GGUF file');
        }

        $result = '';
        while (strlen($result) < $length) {
            $chunk = fread($this->handle, $length - strlen($result));
            if ($chunk === false || $chunk === '') {
                throw new GgufException('Short read from GGUF file');
            }
            $result .= $chunk;
        }

        return $result;
    }

    private function skip(int $length): void
    {
        if ($length < 0) {
            throw new GgufException('Negative skip length');
        }
        $position = $this->position();
        if ($position + $length > $this->fileSize) {
            throw new GgufException('Skip beyond end of GGUF file');
        }
        $this->seekAbsolute($position + $length);
    }

    private function seekAbsolute(int $offset): void
    {
        if ($offset < 0 || $offset > $this->fileSize) {
            throw new GgufException('Seek outside GGUF file');
        }
        if (fseek($this->handle, $offset, SEEK_SET) !== 0) {
            throw new GgufException('Unable to seek in GGUF file');
        }
    }

    private function position(): int
    {
        $position = ftell($this->handle);
        if ($position === false) {
            throw new GgufException('Unable to tell GGUF file position');
        }

        return $position;
    }

    private static function align(int $value, int $alignment): int
    {
        $remainder = $value % $alignment;

        return $remainder === 0 ? $value : $value + $alignment - $remainder;
    }
}
