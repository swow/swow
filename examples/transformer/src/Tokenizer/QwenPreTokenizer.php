<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tokenizer;

use RuntimeException;

use function preg_match_all;
use function preg_quote;
use function strlen;

use const PREG_SPLIT_DELIM_CAPTURE;
use const PREG_SPLIT_NO_EMPTY;

final class QwenPreTokenizer
{
    private const PATTERN = '/(?:\'[sS]|\'[tT]|\'[rR][eE]|\'[vV][eE]|\'[mM]|\'[lL][lL]|\'[dD])|[^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+|\p{N}| ?[^\s\p{L}\p{M}\p{N}]+[\r\n]*|\s*[\r\n]+|\s+(?!\S)|\s+/u';

    /** @return list<string> */
    public function split(string $text): array
    {
        if ($text === '') {
            return [];
        }
        $matches = [];
        if (preg_match_all(self::PATTERN, $text, $matches) === false) {
            throw new RuntimeException('Qwen pre-tokenization failed');
        }

        return $matches[0] ?? [];
    }

    /** @param array<string, int> $specialIds @return list<string|int> */
    public function splitSpecial(string $text, array $specialIds): array
    {
        if ($specialIds === []) {
            return $this->split($text);
        }
        $tokens = array_keys($specialIds);
        usort($tokens, static fn(string $a, string $b): int => strlen($b) <=> strlen($a));
        $pattern = '/(' . implode('|', array_map(static fn(string $token): string => preg_quote($token, '/'), $tokens)) . ')/u';
        $parts = preg_split($pattern, $text, -1, PREG_SPLIT_DELIM_CAPTURE | PREG_SPLIT_NO_EMPTY);
        if ($parts === false) {
            throw new RuntimeException('Special token split failed');
        }
        $result = [];
        foreach ($parts as $part) {
            if (isset($specialIds[$part])) {
                $result[] = $specialIds[$part];
                continue;
            }
            foreach ($this->split($part) as $piece) {
                $result[] = $piece;
            }
        }

        return $result;
    }
}
