<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Gguf;

use function array_key_exists;

final class GgufMetadata
{
    /** @param array<string, mixed> $values */
    public function __construct(private array $values)
    {
    }

    public function has(string $key): bool
    {
        return array_key_exists($key, $this->values);
    }

    public function get(string $key, mixed $default = null): mixed
    {
        return $this->values[$key] ?? $default;
    }

    /** @return array<string, mixed> */
    public function all(): array
    {
        return $this->values;
    }
}
