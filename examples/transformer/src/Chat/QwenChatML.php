<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Chat;

use RuntimeException;

use function explode;
use function in_array;
use function str_contains;
use function trim;

final class QwenChatML
{
    /**
     * @param list<ChatMessage> $messages
     */
    public function render(
        array $messages,
        bool $addGenerationPrompt = true,
        bool $enableThinking = false,
    ): string {
        if ($messages === []) {
            throw new RuntimeException('No chat messages');
        }
        $lastUser = -1;
        foreach ($messages as $index => $message) {
            if (!in_array($message->role, ['system', 'user', 'assistant'], true)) {
                throw new RuntimeException('Unsupported chat role: ' . $message->role);
            }
            if ($message->role === 'user') {
                $lastUser = $index;
            }
        }

        $text = '';
        foreach ($messages as $index => $message) {
            if ($message->role === 'system') {
                if ($index !== 0) {
                    throw new RuntimeException('System message must be first');
                }
                $text .= '<|im_start|>system' . "\n" . trim($message->content) . "<|im_end|>\n";
                continue;
            }
            if ($message->role === 'user') {
                $text .= '<|im_start|>user' . "\n" . trim($message->content) . "<|im_end|>\n";
                continue;
            }

            [$content, $reasoning] = $this->splitAssistantContent($message);
            $text .= '<|im_start|>assistant' . "\n";
            if ($index > $lastUser) {
                $text .= "<think>\n" . trim($reasoning) . "\n</think>\n\n";
            }
            $text .= trim($content) . "<|im_end|>\n";
        }

        if ($addGenerationPrompt) {
            $text .= '<|im_start|>assistant' . "\n";
            $text .= $enableThinking
                ? "<think>\n"
                : "<think>\n\n</think>\n\n";
        }

        return $text;
    }

    public function nextUserTurn(string $content, bool $enableThinking = false): string
    {
        return "<|im_end|>\n"
            . '<|im_start|>user' . "\n" . trim($content) . "<|im_end|>\n"
            . '<|im_start|>assistant' . "\n"
            . ($enableThinking ? "<think>\n" : "<think>\n\n</think>\n\n");
    }

    /** @return array{0: string, 1: string} */
    private function splitAssistantContent(ChatMessage $message): array
    {
        $content = $message->content;
        $reasoning = $message->reasoningContent;
        if ($reasoning === '' && str_contains($content, '</think>')) {
            [$before, $after] = explode('</think>', $content, 2);
            $reasoning = preg_split('/<think>/', $before)[-1] ?? $before;
            $content = $after;
        }

        return [$content, $reasoning];
    }
}
