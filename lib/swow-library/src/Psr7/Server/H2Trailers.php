<?php

declare(strict_types=1);

namespace Swow\Psr7\Server;

final class H2Trailers
{
    /** @var array<string, list<string>> */
    private array $headers;

    /**
     * @param array<string, array<string>|string> $headers
     */
    private function __construct(array $headers)
    {
        $normalized = [];
        foreach ($headers as $name => $value) {
            $values = is_array($value) ? $value : [$value];
            $normalized[$name] = array_map(static fn(mixed $single): string => (string) $single, $values);
        }

        $this->headers = $normalized;
    }

    public static function empty(): self
    {
        return new self([]);
    }

    /**
     * @param array<string, array<string>|string> $headers
     */
    public static function fromArray(array $headers): self
    {
        return new self($headers);
    }

    /**
     * @return array<string, list<string>>
     */
    public function toArray(): array
    {
        return $this->headers;
    }

    public function isEmpty(): bool
    {
        return $this->headers === [];
    }

    /**
     * @return list<string>
     */
    public function get(string $name): array
    {
        return $this->headers[$name] ?? [];
    }
}
