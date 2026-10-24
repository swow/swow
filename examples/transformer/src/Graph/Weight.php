<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

use Swow\Examples\Transformer\Gguf\GgmlType;
use Swow\Examples\Transformer\Gguf\GgufFile;

use function count;
use function hex2bin;
use function intdiv;
use function pack;
use function strlen;

final class Weight
{
    public const F32 = 'f32';

    public const Q8_0 = 'q8_0';

    public const Q4_0 = 'q4_0';

    /** @var array<string, string> */
    private array $slices = [];

    public function __construct(
        private string $name,
        private string $type,
        private int $rows,
        private int $columns,
        private string $data,
        private ?string $sourcePath = null,
        private ?string $tensorName = null,
        private ?GgufFile $source = null,
    ) {
        if ($rows <= 0 || $columns <= 0) {
            throw new GraphException('Weight dimensions must be positive');
        }
        if ($source === null) {
            $expected = self::rowBytesFor($type, $columns) * $rows;
            if (strlen($data) !== $expected) {
                throw new GraphException(
                    "Weight {$name} byte size {$expected} does not match data " . strlen($data),
                );
            }
        }
    }

    public static function f32(string $name, int $rows, int $columns, array $values): self
    {
        if (count($values) !== $rows * $columns) {
            throw new GraphException("Weight {$name} value count mismatch");
        }

        return new self($name, self::F32, $rows, $columns, pack('g*', ...$values));
    }

    public static function packed(string $name, string $type, int $rows, int $columns, string $data): self
    {
        return new self($name, $type, $rows, $columns, $data);
    }

    public static function fromHex(string $name, string $type, int $rows, int $columns, string $hex): self
    {
        $data = hex2bin($hex);
        if ($data === false) {
            throw new GraphException("Invalid weight hex for {$name}");
        }

        return new self($name, $type, $rows, $columns, $data);
    }

    public static function fromGguf(GgufFile $file, string $tensorName): self
    {
        $info = $file->tensor($tensorName);
        $type = match ($info->ggmlType()) {
            GgmlType::F32 => self::F32,
            GgmlType::Q8_0 => self::Q8_0,
            GgmlType::Q4_0 => self::Q4_0,
            default => throw new GraphException('Unsupported GGUF weight type: ' . $info->typeName()),
        };

        return new self(
            $tensorName,
            $type,
            $info->rows(),
            $info->columns(),
            $file->readAll($tensorName),
            $file->path(),
            $tensorName,
        );
    }

    /*
     * A lazy weight reads only the metadata now. The data stays in the file
     * until data() or slice() reads it, so a worker thread can load just the
     * row range it computes.
     */
    public static function fromGgufLazy(GgufFile $file, string $tensorName): self
    {
        $info = $file->tensor($tensorName);
        $type = match ($info->ggmlType()) {
            GgmlType::F32 => self::F32,
            GgmlType::Q8_0 => self::Q8_0,
            GgmlType::Q4_0 => self::Q4_0,
            default => throw new GraphException('Unsupported GGUF weight type: ' . $info->typeName()),
        };

        return new self(
            $tensorName,
            $type,
            $info->rows(),
            $info->columns(),
            '',
            $file->path(),
            $tensorName,
            $file,
        );
    }

    public function isLazy(): bool
    {
        return $this->source !== null;
    }

    public function name(): string
    {
        return $this->name;
    }

    public function sourcePath(): ?string
    {
        return $this->sourcePath;
    }

    public function tensorName(): ?string
    {
        return $this->tensorName;
    }

    public function type(): string
    {
        return $this->type;
    }

    public function rows(): int
    {
        return $this->rows;
    }

    public function columns(): int
    {
        return $this->columns;
    }

    public function rowBytes(): int
    {
        return self::rowBytesFor($this->type, $this->columns);
    }

    public function data(): string
    {
        if ($this->source !== null) {
            $this->data = $this->source->readAll((string) $this->tensorName);
            $this->source = null;
            $this->slices = [];
        }

        return $this->data;
    }

    public function slice(int $begin, int $end): string
    {
        if ($begin < 0 || $end < $begin || $end > $this->rows) {
            throw new GraphException("Weight row range out of range: {$begin}..{$end}");
        }
        if ($begin === $end) {
            return '';
        }
        $key = $begin . ':' . $end;
        if (isset($this->slices[$key])) {
            return $this->slices[$key];
        }
        if ($this->source !== null) {
            return $this->slices[$key] = $this->source->rows((string) $this->tensorName, $begin, $end);
        }
        $rowBytes = $this->rowBytes();

        return $this->slices[$key] = substr($this->data, $begin * $rowBytes, ($end - $begin) * $rowBytes);
    }

    public function row(int $row): string
    {
        if ($row < 0 || $row >= $this->rows) {
            throw new GraphException("Weight row out of range: {$row}");
        }
        if ($this->source !== null) {
            return $this->source->rows((string) $this->tensorName, $row, $row + 1);
        }

        return substr($this->data, $row * $this->rowBytes(), $this->rowBytes());
    }

    private static function rowBytesFor(string $type, int $columns): int
    {
        if ($type === self::F32) {
            return $columns * 4;
        }
        if ($type === self::Q8_0) {
            if ($columns % 32 !== 0) {
                throw new GraphException('Q8_0 columns must be a multiple of 32');
            }

            return intdiv($columns, 32) * 34;
        }
        if ($type === self::Q4_0) {
            if ($columns % 32 !== 0) {
                throw new GraphException('Q4_0 columns must be a multiple of 32');
            }

            return intdiv($columns, 32) * 18;
        }

        throw new GraphException("Unsupported weight type: {$type}");
    }
}
