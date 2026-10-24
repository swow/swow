<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use Swow\Examples\Transformer\Chat\ChatMessage;
use Swow\Examples\Transformer\Chat\QwenChatML;
use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Tokenizer\QwenTokenizer;

use function count;
use function is_file;

/**
 * @internal
 */
final class TokenizerTest extends TestCase
{
    private const MODEL = __DIR__ . '/../models/qwen3.5-0.8b/Qwen3.5-0.8B.Q8_0.gguf';

    private static ?QwenTokenizer $tokenizer = null;

    public function testKnownTokenIds(): void
    {
        $tokenizer = $this->tokenizer();
        $cases = [
            'Hello' => [9419],
            'Hello world' => [9419, 1814],
            '你好' => [109266],
            'Hello 你好 😀' => [9419, 220, 109266, 87209],
            "a\nb" => [64, 198, 65],
        ];
        foreach ($cases as $text => $expected) {
            $this->assertSame($expected, $tokenizer->encode($text), $text);
        }
    }

    public function testRoundTrip(): void
    {
        $tokenizer = $this->tokenizer();
        foreach (['', 'Hello', 'Hello 你好 😀', "a\nb", '  leading and trailing  '] as $text) {
            $this->assertSame($text, $tokenizer->decode($tokenizer->encode($text)), $text);
        }
    }

    public function testSpecialTokensAndChatTemplate(): void
    {
        $tokenizer = $this->tokenizer();
        $text = '<|im_start|>user' . "\n" . 'hi<|im_end|>';
        $ids = $tokenizer->encode($text);
        $this->assertSame(248045, $ids[0]);
        $this->assertSame(248046, $ids[count($ids) - 1]);
        $this->assertSame('user' . "\n" . 'hi', $tokenizer->decode($ids));

        $template = new QwenChatML();
        $prompt = $template->render([
            new ChatMessage('system', 'You are helpful.'),
            new ChatMessage('user', 'Hi'),
        ]);
        $this->assertSame(
            "<|im_start|>system\nYou are helpful.<|im_end|>\n"
            . "<|im_start|>user\nHi<|im_end|>\n"
            . "<|im_start|>assistant\n<think>\n\n</think>\n\n",
            $prompt,
        );
        $promptIds = $tokenizer->encode($prompt);
        $this->assertContains(248045, $promptIds);
        $this->assertContains(248046, $promptIds);
    }

    public function testSpecialIds(): void
    {
        $tokenizer = $this->tokenizer();
        $this->assertSame(248046, $tokenizer->eosId());
        $this->assertSame(248044, $tokenizer->padId());
    }

    private function tokenizer(): QwenTokenizer
    {
        if (!is_file(self::MODEL)) {
            $this->markTestSkipped('Qwen3.5 Q8 tokenizer model is not downloaded');
        }

        return self::$tokenizer ??= QwenTokenizer::fromGguf(GgufFile::open(self::MODEL));
    }
}
