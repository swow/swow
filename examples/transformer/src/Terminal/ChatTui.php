<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Terminal;

use Swow\Examples\Transformer\Chat\ContextLimitException;
use Swow\Examples\Transformer\Chat\Conversation;
use Throwable;

use function preg_match;
use function strlen;
use function substr;
use function trim;

final class ChatTui
{
    public function __construct(
        private XtermTerminal $terminal,
        private Conversation $conversation,
        private ?string $system = null,
    ) {
    }

    public function run(): void
    {
        $this->terminal->enterRawMode();
        try {
            if ($this->system !== null && $this->system !== '') {
                $this->terminal->writeln($this->terminal->systemLabel('System:') . ' ' . $this->system);
                $this->terminal->writeln();
            }
            while (true) {
                $line = $this->terminal->readLine(' ' . $this->terminal->promptMark('>') . ' ');
                if ($line === null) {
                    break;
                }
                if ($line === false) {
                    continue;
                }
                $line = trim($line);
                if ($line === '') {
                    continue;
                }

                $assistantPrefix = ' ' . $this->terminal->assistantLabel('Assistant:') . ' ';
                $this->terminal->write($assistantPrefix . $this->terminal->dim('...'));

                $started = false;
                $displayBuffer = '';
                $onToken = function (string $piece) use ($assistantPrefix, &$started, &$displayBuffer): void {
                    if (!$started) {
                        $this->terminal->write("\r\x1b[2K" . $assistantPrefix);
                        $started = true;
                    }
                    $displayBuffer .= $piece;
                    while (preg_match(
                        '/\A(?:[\x00-\x7F]|[\xC2-\xDF][\x80-\xBF]|[\xE0-\xEF][\x80-\xBF]{2}|[\xF0-\xF4][\x80-\xBF]{3})/',
                        $displayBuffer,
                        $matches,
                    ) === 1) {
                        $this->terminal->write($matches[0]);
                        $displayBuffer = substr($displayBuffer, strlen($matches[0]));
                    }
                };

                try {
                    $this->conversation->respond($line, $onToken);
                } catch (ContextLimitException $e) {
                    $this->terminal->write("\r\x1b[2K");
                    $this->terminal->writeln($this->terminal->style($e->getMessage(), '31;1'));
                    $this->terminal->writeln();
                    continue;
                } catch (Throwable $e) {
                    $this->terminal->write("\r\x1b[2K");
                    $this->terminal->writeln($this->terminal->style($e::class . ': ' . $e->getMessage(), '31;1'));
                    $this->terminal->writeln();
                    continue;
                }
                if (!$started) {
                    $this->terminal->write("\r\x1b[2K" . $assistantPrefix);
                }
                if ($displayBuffer !== '') {
                    $this->terminal->write($displayBuffer);
                }
                $this->terminal->writeln();
                $this->terminal->writeln();
            }
        } finally {
            $this->terminal->restore();
        }
    }
}
