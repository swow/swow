<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tokenizer;

use RuntimeException;

use function chr;
use function count;
use function function_exists;
use function in_array;
use function mb_chr;
use function ord;
use function preg_split;
use function strlen;

use const PREG_SPLIT_NO_EMPTY;

final class ByteLevelBpe
{
    /** @var array<int, string> */
    private array $byteToChar = [];

    /** @var array<string, int> */
    private array $charToByte = [];

    /** @var array<string, list<int>> */
    private array $cache = [];

    public function __construct(
        private Vocabulary $vocabulary,
        private MergeTable $merges,
    ) {
        $this->buildByteMap();
    }

    /** @param list<string> $pieces @return list<int> */
    public function encodePieces(array $pieces): array
    {
        $ids = [];
        foreach ($pieces as $piece) {
            foreach ($this->encodePiece($piece) as $id) {
                $ids[] = $id;
            }
        }

        return $ids;
    }

    /** @param list<int> $ids */
    public function decode(array $ids, bool $skipSpecial = true): string
    {
        $bytes = '';
        foreach ($ids as $id) {
            if ($this->vocabulary->isSpecial($id)) {
                if (!$skipSpecial) {
                    $bytes .= $this->vocabulary->token($id);
                }
                continue;
            }
            $token = $this->vocabulary->token($id);
            foreach (preg_split('//u', $token, -1, PREG_SPLIT_NO_EMPTY) ?: [] as $char) {
                if (!isset($this->charToByte[$char])) {
                    throw new RuntimeException('Unknown byte-level character in token');
                }
                $bytes .= chr($this->charToByte[$char]);
            }
        }

        return $bytes;
    }

    /** @return list<int> */
    private function encodePiece(string $piece): array
    {
        if (isset($this->cache[$piece])) {
            return $this->cache[$piece];
        }
        $bytes = '';
        for ($index = 0, $length = strlen($piece); $index < $length; $index++) {
            $bytes .= $this->byteToChar[ord($piece[$index])];
        }
        $symbols = preg_split('//u', $bytes, -1, PREG_SPLIT_NO_EMPTY);
        if ($symbols === false) {
            throw new RuntimeException('Unable to split byte-level string');
        }
        while (count($symbols) > 1) {
            $bestRank = null;
            $bestIndex = -1;
            for ($index = 0, $last = count($symbols) - 1; $index < $last; $index++) {
                $rank = $this->merges->rank($symbols[$index], $symbols[$index + 1]);
                if ($rank !== null && ($bestRank === null || $rank < $bestRank)) {
                    $bestRank = $rank;
                    $bestIndex = $index;
                }
            }
            if ($bestIndex < 0) {
                break;
            }
            $symbols[$bestIndex] .= $symbols[$bestIndex + 1];
            array_splice($symbols, $bestIndex + 1, 1);
        }
        $ids = [];
        foreach ($symbols as $symbol) {
            $id = $this->vocabulary->id($symbol);
            if ($id === null) {
                throw new RuntimeException('Token not found in vocabulary: ' . $symbol);
            }
            $ids[] = $id;
        }

        return $this->cache[$piece] = $ids;
    }

    private function buildByteMap(): void
    {
        $printable = [];
        for ($byte = 33; $byte <= 126; $byte++) {
            $printable[] = $byte;
        }
        for ($byte = 161; $byte <= 172; $byte++) {
            $printable[] = $byte;
        }
        for ($byte = 174; $byte <= 255; $byte++) {
            $printable[] = $byte;
        }
        $codepoints = $printable;
        $extra = 0;
        foreach (range(0, 255) as $byte) {
            if (!in_array($byte, $printable, true)) {
                $codepoints[] = 256 + $extra++;
            }
        }
        foreach ($printable as $index => $byte) {
            $char = $this->utf8Chr($codepoints[$index]);
            $this->byteToChar[$byte] = $char;
            $this->charToByte[$char] = $byte;
        }
        $printableCount = count($printable);
        $byteIndex = 0;
        foreach (range(0, 255) as $byte) {
            if (in_array($byte, $printable, true)) {
                continue;
            }
            $char = $this->utf8Chr($codepoints[$printableCount + $byteIndex]);
            $this->byteToChar[$byte] = $char;
            $this->charToByte[$char] = $byte;
            $byteIndex++;
        }
    }

    private function utf8Chr(int $codepoint): string
    {
        if (function_exists('mb_chr')) {
            return mb_chr($codepoint, 'UTF-8');
        }
        if ($codepoint <= 0x7F) {
            return chr($codepoint);
        }
        if ($codepoint <= 0x7FF) {
            return chr(0xC0 | ($codepoint >> 6)) . chr(0x80 | ($codepoint & 0x3F));
        }

        return chr(0xE0 | ($codepoint >> 12))
            . chr(0x80 | (($codepoint >> 6) & 0x3F))
            . chr(0x80 | ($codepoint & 0x3F));
    }
}
