<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use RuntimeException;
use Swow\Examples\Transformer\Chat\ChatMessage;
use Swow\Examples\Transformer\Chat\ChatSession;
use Swow\Examples\Transformer\Chat\QwenChatML;

/**
 * @internal
 */
final class ChatMLTest extends TestCase
{
    public function testPlainPrompt(): void
    {
        $text = (new QwenChatML())->render([new ChatMessage('user', 'Hi')]);
        $this->assertSame(
            "<|im_start|>user\nHi<|im_end|>\n"
            . "<|im_start|>assistant\n<think>\n\n</think>\n\n",
            $text,
        );
    }

    public function testNextUserTurn(): void
    {
        $text = (new QwenChatML())->nextUserTurn('How are you?');
        $this->assertSame(
            "<|im_end|>\n"
            . "<|im_start|>user\nHow are you?<|im_end|>\n"
            . "<|im_start|>assistant\n<think>\n\n</think>\n\n",
            $text,
        );
    }

    public function testThinkingPromptAndSystem(): void
    {
        $session = (new ChatSession())->system('You are helpful.')->user('Hi');
        $text = (new QwenChatML())->render($session->messages(), true, true);
        $this->assertSame(
            "<|im_start|>system\nYou are helpful.<|im_end|>\n"
            . "<|im_start|>user\nHi<|im_end|>\n"
            . "<|im_start|>assistant\n<think>\n",
            $text,
        );
    }

    public function testAssistantHistoryAndReasoning(): void
    {
        $messages = [
            new ChatMessage('user', 'Hi'),
            new ChatMessage('assistant', 'Hello', 'greeting'),
            new ChatMessage('user', 'How are you?'),
        ];
        $text = (new QwenChatML())->render($messages, false);
        $this->assertSame(
            "<|im_start|>user\nHi<|im_end|>\n"
            . "<|im_start|>assistant\nHello<|im_end|>\n"
            . "<|im_start|>user\nHow are you?<|im_end|>\n",
            $text,
        );
    }

    public function testSystemMustBeFirst(): void
    {
        $this->expectException(RuntimeException::class);
        (new QwenChatML())->render([
            new ChatMessage('user', 'Hi'),
            new ChatMessage('system', 'late'),
        ]);
    }
}
