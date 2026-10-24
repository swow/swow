<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Chat;

use RuntimeException;

final class ChatSession
{
    /** @var list<ChatMessage> */
    private array $messages = [];

    public function system(string $content): self
    {
        if ($this->messages !== []) {
            throw new RuntimeException('System message must be first');
        }
        $this->messages[] = new ChatMessage('system', $content);

        return $this;
    }

    public function user(string $content): self
    {
        $this->messages[] = new ChatMessage('user', $content);

        return $this;
    }

    public function assistant(string $content, string $reasoningContent = ''): self
    {
        $this->messages[] = new ChatMessage('assistant', $content, $reasoningContent);

        return $this;
    }

    /** @return list<ChatMessage> */
    public function messages(): array
    {
        return $this->messages;
    }
}
