<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use Swow\Examples\Transformer\Chat\ChatMessage;
use Swow\Examples\Transformer\Chat\ContextLimitException;
use Swow\Examples\Transformer\Chat\Conversation;
use Swow\Examples\Transformer\Chat\QwenChatML;
use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Inference\Inferencer;
use Swow\Examples\Transformer\Sampler\Sampler;
use Swow\Examples\Transformer\Sampler\SamplingConfig;
use Swow\Examples\Transformer\Tokenizer\QwenTokenizer;

use function array_fill;
use function array_slice;
use function count;
use function is_file;

use const INF;

/**
 * @internal
 */
final class ConversationTest extends TestCase
{
    private const MODEL = __DIR__ . '/../models/qwen3.5-0.8b/Qwen3.5-0.8B.Q8_0.gguf';

    private static ?QwenTokenizer $tokenizer = null;

    public function testFirstTurnAndMessages(): void
    {
        $tokenizer = $this->tokenizer();
        $fake = $this->fake($tokenizer, 9419);
        $conversation = new Conversation(
            $fake,
            $tokenizer,
            new Sampler(new SamplingConfig(temperature: 0.0)),
            2048,
            1,
            false,
            'You are helpful.',
        );

        $answer = $conversation->respond('Hi');
        $this->assertSame('Hello', $answer);
        $messages = $conversation->messages();
        $this->assertCount(3, $messages);
        $this->assertSame('system', $messages[0]->role);
        $this->assertSame('user', $messages[1]->role);
        $this->assertSame('assistant', $messages[2]->role);

        $expectedPrompt = (new QwenChatML())->render([
            new ChatMessage('system', 'You are helpful.'),
            new ChatMessage('user', 'Hi'),
        ], true, false);
        $expected = $tokenizer->encode($expectedPrompt);
        $this->assertSame($expected, array_slice($fake->tokens, 0, count($expected)));
    }

    public function testStopTokenProducesEmptyReply(): void
    {
        $tokenizer = $this->tokenizer();
        $conversation = new Conversation(
            $this->fake($tokenizer, $tokenizer->eosId()),
            $tokenizer,
            new Sampler(new SamplingConfig(temperature: 0.0)),
            2048,
            4,
            false,
            'You are helpful.',
        );

        $this->assertSame('', $conversation->respond('Hi'));
        $messages = $conversation->messages();
        $this->assertSame('assistant', $messages[2]->role);
        $this->assertSame('', $messages[2]->content);
    }

    public function testSecondTurnUsesSuffix(): void
    {
        $tokenizer = $this->tokenizer();
        $fake = $this->fake($tokenizer, 9419);
        $conversation = new Conversation(
            $fake,
            $tokenizer,
            new Sampler(new SamplingConfig(temperature: 0.0)),
            2048,
            1,
            false,
            'You are helpful.',
        );
        $conversation->respond('Hi');
        $firstCallCount = count($fake->tokens);
        $conversation->respond('How are you?');

        $suffix = $tokenizer->encode((new QwenChatML())->nextUserTurn('How are you?'));
        $this->assertSame($suffix, array_slice($fake->tokens, $firstCallCount, count($suffix)));
        $this->assertCount(5, $conversation->messages());
    }

    public function testContextLimit(): void
    {
        $tokenizer = $this->tokenizer();
        $conversation = new Conversation(
            $this->fake($tokenizer, 9419),
            $tokenizer,
            new Sampler(new SamplingConfig(temperature: 0.0)),
            1,
            1,
            false,
            'You are helpful.',
        );

        $this->expectException(ContextLimitException::class);
        $conversation->respond('Hi');
    }

    private function fake(QwenTokenizer $tokenizer, int $token): object
    {
        return new class($tokenizer->vocabulary()->size(), $token) implements Inferencer {
            /** @var list<int> */
            public array $tokens = [];

            public function __construct(
                private int $vocabularySize,
                private int $token,
            ) {
            }

            public function forward(int $token, int $position): array
            {
                $this->tokens[] = $token;
                $logits = array_fill(0, $this->vocabularySize, -INF);
                $logits[$this->token] = 100.0;

                return $logits;
            }

            public function reset(): void
            {
            }
        };
    }

    private function tokenizer(): QwenTokenizer
    {
        if (!is_file(self::MODEL)) {
            $this->markTestSkipped('Qwen3.5 Q8 tokenizer model is not downloaded');
        }

        return self::$tokenizer ??= QwenTokenizer::fromGguf(GgufFile::open(self::MODEL));
    }
}
