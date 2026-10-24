<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tokenizer;

use RuntimeException;
use Swow\Examples\Transformer\Gguf\GgufArrayRef;
use Swow\Examples\Transformer\Gguf\GgufFile;

use function array_values;
use function is_array;
use function is_int;

final class QwenTokenizer
{
    private Vocabulary $vocabulary;
    private ByteLevelBpe $bpe;
    private QwenPreTokenizer $preTokenizer;

    /** @param list<string> $tokens @param list<int> $types @param list<string> $merges */
    public function __construct(
        array $tokens,
        array $types,
        array $merges,
        private int $eosId,
        private ?int $padId = null,
        private ?int $bosId = null,
    ) {
        $this->vocabulary = new Vocabulary($tokens, $types);
        $this->bpe = new ByteLevelBpe($this->vocabulary, new MergeTable($merges));
        $this->preTokenizer = new QwenPreTokenizer();
    }

    public static function fromGguf(GgufFile $file): self
    {
        $metadata = $file->metadata();
        $tokens = self::loadArray($metadata->get('tokenizer.ggml.tokens'), 'tokenizer.ggml.tokens');
        $types = self::loadArray($metadata->get('tokenizer.ggml.token_type'), 'tokenizer.ggml.token_type');
        $merges = self::loadArray($metadata->get('tokenizer.ggml.merges'), 'tokenizer.ggml.merges');
        $eos = $metadata->get('tokenizer.ggml.eos_token_id');
        if (!is_int($eos)) {
            throw new RuntimeException('Tokenizer EOS id is missing');
        }
        $pad = $metadata->get('tokenizer.ggml.padding_token_id');
        $bos = $metadata->get('tokenizer.ggml.bos_token_id');

        return new self(
            $tokens,
            array_map(static fn(mixed $value): int => (int) $value, $types),
            $merges,
            $eos,
            is_int($pad) ? $pad : null,
            is_int($bos) ? $bos : null,
        );
    }

    /** @return list<int> */
    public function encode(string $text, bool $addSpecial = false): array
    {
        $parts = $this->preTokenizer->splitSpecial($text, $this->vocabulary->specialIds());
        $ids = [];
        foreach ($parts as $part) {
            if (is_int($part)) {
                $ids[] = $part;
                continue;
            }
            foreach ($this->bpe->encodePieces([$part]) as $id) {
                $ids[] = $id;
            }
        }
        if ($addSpecial && $this->bosId !== null) {
            array_unshift($ids, $this->bosId);
        }

        return $ids;
    }

    /** @param list<int> $ids */
    public function decode(array $ids, bool $skipSpecial = true): string
    {
        return $this->bpe->decode($ids, $skipSpecial);
    }

    public function eosId(): int
    {
        return $this->eosId;
    }

    public function padId(): ?int
    {
        return $this->padId;
    }

    public function bosId(): ?int
    {
        return $this->bosId;
    }

    public function vocabulary(): Vocabulary
    {
        return $this->vocabulary;
    }

    /** @return list<mixed> */
    private static function loadArray(mixed $value, string $name): array
    {
        if ($value instanceof GgufArrayRef) {
            return $value->toArray();
        }
        if (!is_array($value)) {
            throw new RuntimeException("Tokenizer metadata is not an array: {$name}");
        }

        return array_values($value);
    }
}
