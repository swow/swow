<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tokenizer;

use RuntimeException;

use function in_array;
use function is_string;

final class QwenChatTemplate
{
    /**
     * @param list<array{role: string, content: string}> $messages
     */
    public function render(array $messages, bool $addGenerationPrompt = true): string
    {
        if ($messages === []) {
            throw new RuntimeException('No chat messages');
        }
        $text = '';
        foreach ($messages as $message) {
            $role = $message['role'] ?? '';
            $content = $message['content'] ?? null;
            if (!is_string($role) || !is_string($content)) {
                throw new RuntimeException('Only text chat messages are supported');
            }
            if (!in_array($role, ['system', 'user', 'assistant'], true)) {
                throw new RuntimeException("Unsupported chat role: {$role}");
            }
            $text .= '<|im_start|>' . $role . "\n" . $content . "<|im_end|>\n";
        }
        if ($addGenerationPrompt) {
            $text .= "<|im_start|>assistant\n";
        }

        return $text;
    }
}
