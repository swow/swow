<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tokenizer;

use RuntimeException;

use function array_flip;
use function count;

final class Vocabulary
{
    /** @var list<string> */
    private array $tokens;

    /** @var list<int> */
    private array $types;

    /** @var array<string, int> */
    private array $ids;

    /** @var array<string, int> */
    private array $specialIds = [];

    /**
     * @param list<string> $tokens
     * @param list<int> $types
     */
    public function __construct(array $tokens, array $types)
    {
        if (count($tokens) !== count($types)) {
            throw new RuntimeException('Token and token type counts differ');
        }
        $this->tokens = $tokens;
        $this->types = $types;
        $this->ids = array_flip($tokens);
        foreach ($tokens as $id => $token) {
            if ($this->isSpecialType($types[$id])) {
                $this->specialIds[$token] = $id;
            }
        }
    }

    public function size(): int
    {
        return count($this->tokens);
    }

    public function id(string $token): ?int
    {
        return $this->ids[$token] ?? null;
    }

    public function token(int $id): string
    {
        return $this->tokens[$id] ?? throw new RuntimeException("Token id out of range: {$id}");
    }

    public function type(int $id): int
    {
        return $this->types[$id] ?? throw new RuntimeException("Token id out of range: {$id}");
    }

    public function isSpecial(int $id): bool
    {
        return $this->isSpecialType($this->type($id));
    }

    /** @return array<string, int> */
    public function specialIds(): array
    {
        return $this->specialIds;
    }

    private function isSpecialType(int $type): bool
    {
        return $type === 3 || $type === 4;
    }
}
