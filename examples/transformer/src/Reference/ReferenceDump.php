<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Reference;

use RuntimeException;

use function array_slice;
use function array_values;
use function file_get_contents;
use function is_file;
use function strlen;
use function substr;
use function unpack;

final class ReferenceDump
{
    /** @var array<string, ReferenceTensor> */
    private array $tensors = [];

    private function __construct()
    {
    }

    public static function open(string $path): self
    {
        if (!is_file($path)) {
            throw new RuntimeException("Reference dump does not exist: {$path}");
        }
        $bytes = file_get_contents($path);
        if ($bytes === false) {
            throw new RuntimeException("Unable to read reference dump: {$path}");
        }

        $dump = new self();
        $offset = 0;
        $length = strlen($bytes);
        while ($offset < $length) {
            $nameLength = unpack('V', substr($bytes, $offset, 4))[1];
            $offset += 4;
            $name = substr($bytes, $offset, $nameLength);
            $offset += $nameLength;
            $dimensionCount = unpack('V', substr($bytes, $offset, 4))[1];
            $offset += 4;
            $dimensions = array_values(unpack('P4', substr($bytes, $offset, 32)));
            $offset += 32;
            $elements = 1;
            for ($dimension = 0; $dimension < $dimensionCount; $dimension++) {
                $elements *= $dimensions[$dimension];
            }
            $raw = substr($bytes, $offset, $elements * 4);
            $offset += $elements * 4;
            $data = array_values(unpack('g*', $raw));
            $dump->tensors[$name] = new ReferenceTensor(
                $name,
                array_slice($dimensions, 0, $dimensionCount),
                $data,
            );
        }

        return $dump;
    }

    public function has(string $name): bool
    {
        return isset($this->tensors[$name]);
    }

    public function tensor(string $name): ReferenceTensor
    {
        if (!isset($this->tensors[$name])) {
            throw new RuntimeException("Unknown reference tensor: {$name}");
        }

        return $this->tensors[$name];
    }

    /** @return list<string> */
    public function names(): array
    {
        return array_keys($this->tensors);
    }
}
