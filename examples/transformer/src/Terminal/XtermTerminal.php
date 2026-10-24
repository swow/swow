<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Terminal;

use RuntimeException;

use function feof;
use function fread;
use function fwrite;
use function ord;
use function shell_exec;
use function substr;
use function trim;

use const STDIN;
use const STDOUT;

final class XtermTerminal
{
    private const ERASE_LINE = "\x1b[2K";

    private ?string $savedModes = null;

    public function __construct()
    {
        if (!stream_isatty(STDIN)) {
            throw new RuntimeException('Chat requires a TTY on stdin');
        }
    }

    public function enterRawMode(): void
    {
        if ($this->savedModes !== null) {
            return;
        }
        $saved = shell_exec('stty -g < /dev/tty 2>/dev/null');
        if ($saved === null || trim($saved) === '') {
            throw new RuntimeException('Unable to read terminal modes');
        }
        $this->savedModes = trim($saved);
        shell_exec('stty -icanon -echo -isig -ixon < /dev/tty 2>/dev/null');
    }

    public function restore(): void
    {
        if ($this->savedModes === null) {
            return;
        }
        shell_exec('stty ' . $this->savedModes . ' < /dev/tty 2>/dev/null');
        $this->savedModes = null;
        $this->showCursor();
    }

    public function __destruct()
    {
        $this->restore();
    }

    /** @return string|false|null string, false on Ctrl-C, null on Ctrl-D */
    public function readLine(string $prompt = ''): string|false|null
    {
        $this->write($prompt);
        $buffer = '';
        while (true) {
            $char = fread(STDIN, 1);
            if ($char === false || $char === '') {
                if (feof(STDIN)) {
                    $this->write("\n");

                    return null;
                }
                continue;
            }
            $byte = ord($char);
            if ($byte === 0x03) {
                $this->write("\r" . self::ERASE_LINE);
                return false;
            }
            if ($byte === 0x04) {
                $this->write("\r" . self::ERASE_LINE . "\n");
                return null;
            }
            if ($byte === 0x0A || $byte === 0x0D) {
                $this->write("\n");
                return $buffer;
            }
            if ($byte === 0x7F || $byte === 0x08) {
                $buffer = $this->removeLastCharacter($buffer);
                $this->redraw($prompt, $buffer);
                continue;
            }
            if ($byte === 0x15) {
                $buffer = '';
                $this->redraw($prompt, $buffer);
                continue;
            }
            if ($byte === 0x0C) {
                $this->clearScreen();
                $this->redraw($prompt, $buffer);
                continue;
            }
            if ($byte === 0x1B) {
                $this->consumeEscapeSequence();
                continue;
            }
            if ($byte < 0x20) {
                continue;
            }
            $buffer .= $char;
            $this->write($char);
        }
    }

    public function write(string $text): void
    {
        fwrite(STDOUT, $text);
        fflush(STDOUT);
    }

    public function writeln(string $text = ''): void
    {
        $this->write($text . "\n");
    }

    public function style(string $text, string $sgr): string
    {
        return "\x1b[" . $sgr . 'm' . $text . "\x1b[0m";
    }

    public function bold(string $text): string
    {
        return $this->style($text, '1');
    }

    public function systemLabel(string $text): string
    {
        return $this->style($text, '37;1');
    }

    public function assistantLabel(string $text): string
    {
        return $this->style($text, '36;1');
    }

    public function promptMark(string $text): string
    {
        return $this->style($text, '32;1');
    }

    public function dim(string $text): string
    {
        return $this->style($text, '2');
    }

    public function clearLine(): void
    {
        $this->write("\r" . self::ERASE_LINE);
    }

    public function clearScreen(): void
    {
        $this->write("\x1b[2J\x1b[H");
    }

    public function hideCursor(): void
    {
        $this->write("\x1b[?25l");
    }

    public function showCursor(): void
    {
        $this->write("\x1b[?25h");
    }

    private function redraw(string $prompt, string $buffer): void
    {
        $this->write("\r" . self::ERASE_LINE . $prompt . $buffer);
    }

    private function removeLastCharacter(string $buffer): string
    {
        if ($buffer === '') {
            return '';
        }
        $buffer = substr($buffer, 0, -1);
        while ($buffer !== '' && (ord(substr($buffer, -1)) & 0xC0) === 0x80) {
            $buffer = substr($buffer, 0, -1);
        }
        if ($buffer !== '' && (ord(substr($buffer, -1)) & 0xC0) === 0xC0) {
            $buffer = substr($buffer, 0, -1);
        }

        return $buffer;
    }

    private function consumeEscapeSequence(): void
    {
        $next = fread(STDIN, 1);
        if ($next !== '[') {
            return;
        }
        while (true) {
            $char = fread(STDIN, 1);
            if ($char === false || $char === '') {
                return;
            }
            $byte = ord($char);
            if ($byte >= 0x40 && $byte <= 0x7E) {
                return;
            }
        }
    }
}
