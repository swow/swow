<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Chat;

use Swow\Examples\Transformer\Inference\Inferencer;
use Swow\Examples\Transformer\Sampler\Sampler;
use Swow\Examples\Transformer\Sampler\SamplingContext;
use Swow\Examples\Transformer\Tokenizer\QwenTokenizer;

use function count;
use function in_array;

final class Conversation
{
    private ChatSession $session;

    /** @var list<int> */
    private array $contextIds = [];

    private int $position = 0;

    private bool $firstTurn = true;

    private QwenChatML $chat;

    public function __construct(
        private Inferencer $inferencer,
        private QwenTokenizer $tokenizer,
        private Sampler $sampler,
        private int $contextLimit = 2048,
        private int $maxTokens = 32,
        private bool $thinking = false,
        ?string $system = null,
        ?QwenChatML $chat = null,
    ) {
        $this->chat = $chat ?? new QwenChatML();
        $this->session = new ChatSession();
        if ($system !== null && $system !== '') {
            $this->session->system($system);
        }
    }

    /**
     * @param null|callable(string): void $onToken
     */
    public function respond(string $userText, ?callable $onToken = null): string
    {
        $userText = trim($userText);
        if ($this->firstTurn) {
            $messages = $this->session->messages();
            $messages[] = new ChatMessage('user', $userText);
            $promptText = $this->chat->render($messages, true, $this->thinking);
        } else {
            $promptText = $this->chat->nextUserTurn($userText, $this->thinking);
        }

        $inputIds = $this->tokenizer->encode($promptText);
        if ($this->position + count($inputIds) > $this->contextLimit) {
            throw new ContextLimitException($this->position, count($inputIds), $this->contextLimit);
        }
        $this->session->user($userText);
        $this->firstTurn = false;

        $logits = null;
        foreach ($inputIds as $token) {
            $logits = $this->inferencer->forward($token, $this->position++);
            $this->contextIds[] = $token;
        }
        if ($logits === null) {
            $this->session->assistant('');

            return '';
        }

        $assistant = '';
        $samplingContext = new SamplingContext(history: $this->contextIds, position: $this->position);
        $stopTokens = [$this->tokenizer->eosId()];
        if ($this->tokenizer->padId() !== null) {
            $stopTokens[] = $this->tokenizer->padId();
        }

        for ($iteration = 0; $iteration < $this->maxTokens && $this->position < $this->contextLimit; $iteration++) {
            $token = $this->sampler->sample($logits, $samplingContext);
            if (in_array($token, $stopTokens, true)) {
                break;
            }
            $this->contextIds[] = $token;
            $samplingContext->history = $this->contextIds;
            $piece = $this->tokenizer->decode([$token]);
            $assistant .= $piece;
            if ($onToken !== null) {
                $onToken($piece);
            }
            $logits = $this->inferencer->forward($token, $this->position++);
        }
        $this->session->assistant($assistant);

        return $assistant;
    }

    /** @return list<ChatMessage> */
    public function messages(): array
    {
        return $this->session->messages();
    }

    public function position(): int
    {
        return $this->position;
    }
}
