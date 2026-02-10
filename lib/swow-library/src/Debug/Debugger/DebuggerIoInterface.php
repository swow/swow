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
 * Date: 2024/3/24 19:26
 */

namespace Swow\Debug\Debugger;

use Swow\Buffer;

interface DebuggerIoInterface
{
    public function in(): string;

    /** @param string|non-empty-array<string|Stringable|Buffer|array{0: string|Stringable|Buffer, 1?: int, 2?: int}|null> $data */
    public function out(string|array $data): static;

    /** @param string|non-empty-array<string|Stringable|Buffer|array{0: string|Stringable|Buffer, 1?: int, 2?: int}|null> $data */
    public function error(string|array $data): static;

    public function isTty(): bool;

    public function quit(): void;
}
