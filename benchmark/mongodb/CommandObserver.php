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

namespace Swow\Benchmark\MongoDB;

use MongoDB\Driver\Monitoring\CommandFailedEvent;
use MongoDB\Driver\Monitoring\CommandStartedEvent;
use MongoDB\Driver\Monitoring\CommandSubscriber;
use MongoDB\Driver\Monitoring\CommandSucceededEvent;

final class CommandObserver implements CommandSubscriber
{
    private int $pending = 0;
    private int $maximumPending = 0;
    private int $completed = 0;
    private int $failed = 0;
    private ?string $firstFailure = null;

    public function __construct(private string $commandName = 'find')
    {
    }

    public function commandStarted(CommandStartedEvent $event): void
    {
        if ($event->getCommandName() === $this->commandName) {
            $this->pending++;
            $this->maximumPending = max($this->maximumPending, $this->pending);
        }
    }

    public function commandSucceeded(CommandSucceededEvent $event): void
    {
        if ($event->getCommandName() === $this->commandName) {
            $this->pending--;
            $this->completed++;
        }
    }

    public function commandFailed(CommandFailedEvent $event): void
    {
        if ($event->getCommandName() === $this->commandName) {
            $this->pending--;
            $this->failed++;
            $this->firstFailure ??= $event->getError()->getMessage();
        }
    }

    /** @return array<string, int|string|null> */
    public function statistics(): array
    {
        // Native command events delimit actual commands, rather than counting
        // coroutines that are merely waiting for a shared client's ownership.
        return [
            $this->commandName . '_max_in_flight' => $this->maximumPending,
            $this->commandName . '_pending' => $this->pending,
            $this->commandName . '_completed' => $this->completed,
            $this->commandName . '_failed' => $this->failed,
            $this->commandName . '_first_failure' => $this->firstFailure,
        ];
    }
}
