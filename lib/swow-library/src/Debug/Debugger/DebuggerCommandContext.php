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
 * Date: 2024/3/24 04:17
 */

namespace Swow\Debug\Debugger;

class DebuggerCommandContext
{
    public const RETURN_TO_NONE = 0;
    public const RETURN_TO_NEXT = 1;

    protected int $returnTo = self::RETURN_TO_NONE;

    public function __construct(
        protected string $command,
        /** @var string[] */
        protected array $arguments,
    ) {
    }

    public function getCommand(): string
    {
        return $this->command;
    }

    public function getArgument(int $index): ?string
    {
        return $this->arguments[$index] ?? null;
    }

    public function getArguments(): array
    {
        return $this->arguments;
    }

    public function getReturnTo(): int
    {
        return $this->returnTo;
    }

    public function setReturnTo(int $returnTo): void
    {
        $this->returnTo = $returnTo;
    }
}
