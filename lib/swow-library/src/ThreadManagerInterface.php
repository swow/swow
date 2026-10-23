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

namespace Swow;

use Closure;

interface ThreadManagerInterface
{
    public function start(Closure $onWorkerStart): void;

    public function stop(): void;

    public function reload(): void;

    public function getWorkerCount(): int;

    public function setMaxRestartCount(int $count): static;

    public function setMaxWaitTime(int $seconds): static;

    public function onWorkerExit(Closure $callback): static;
}
