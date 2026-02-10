<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);
/**
 * Author: Twosee <twose@qq.com>
 * Date: 2024/3/24 02:05
 */

namespace Swow\Debug\Debugger;

use Swow\Buffer;

use function count;
use function explode;
use function sprintf;
use function trim;

trait DebuggerIoTrait
{
    protected DebuggerIoInterface $io;

    protected array $outputBuffer = [];

    public function __constructDebuggerIo(DebuggerIoInterface $io): void
    {
        $this->io = $io;
    }

    public function in(bool $prefix = true): array
    {
        $line = $this->io->in();
        $parts = explode(' ', $line);
        $arguments = [];
        foreach ($parts as $part) {
            $argument = trim($part);
            if ($argument === '') {
                continue;
            }
            $arguments[] = $argument;
        }
        return $arguments;
    }

    /** @param string|non-empty-array<string|Stringable|Buffer|null> $data */
    public function out(string|array $data): static
    {
        $this->io->out($data);
        return $this;
    }

    public function exception(string|array $data): static
    {
        $this->io->out($data);
        return $this;
    }

    public function error(string|array $data): static
    {
        $this->io->error($data);
        return $this;
    }

    public function flush(): static
    {
        $this->io->out($this->outputBuffer);
        $this->outputBuffer = [];
        return $this;
    }

    public function cr(): static
    {
        $lastLineIndex = count($this->outputBuffer) - 1;

        return $this->out("\r");
    }

    public function lf(): static
    {
        return $this->out("\n");
    }

    public function clear(): static
    {
        if ($this->io->isTty()) {
            $this->io->out("\033c\n");
        }
        return $this;
    }

    public function quit(): void
    {
        $this->io->quit();
    }

    /**
     * @param array<mixed> $table
     */
    public function table(array $table): static
    {
        return $this->out(DebuggerHelper::tableFormat($table));
    }

    protected function setCursorVisibility(bool $bool): static
    {
        if ($this->io->isTty()) {
            $this->out(sprintf("\033[?25%s\n", $bool ? 'h' : 'l'));
        }
        return $this;
    }
}
