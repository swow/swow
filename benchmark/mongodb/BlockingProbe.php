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

use MongoDB\Driver\BulkWrite;
use MongoDB\Driver\Command;
use MongoDB\Driver\Manager;
use MongoDB\Driver\Monitoring\CommandFailedEvent;
use MongoDB\Driver\Monitoring\CommandStartedEvent;
use MongoDB\Driver\Monitoring\CommandSubscriber;
use MongoDB\Driver\Monitoring\CommandSucceededEvent;
use MongoDB\Driver\Query;
use RuntimeException;
use Swow\Channel;
use Swow\Coroutine;
use Swow\Extension;
use Throwable;

use function count;
use function ini_get;
use function MongoDB\Driver\Monitoring\addSubscriber;
use function MongoDB\Driver\Monitoring\removeSubscriber;
use function sprintf;

final class BlockingProbe implements CommandSubscriber
{
    private bool $queryActive = false;
    private int $queryStarted = 0;
    private float $queryMs = 0;
    private int $queryEvents = 0;
    private int $ticksDuringQuery = 0;
    private float $maximumTickGapMs = 0;

    public function commandStarted(CommandStartedEvent $event): void
    {
        if ($event->getCommandName() === 'find') {
            $this->queryStarted = hrtime(true);
            $this->queryActive = true;
            $this->queryEvents++;
        }
    }

    public function commandSucceeded(CommandSucceededEvent $event): void
    {
        if ($event->getCommandName() === 'find') {
            $this->queryMs = (hrtime(true) - $this->queryStarted) / 1e6;
            $this->queryActive = false;
        }
    }

    public function commandFailed(CommandFailedEvent $event): void
    {
        $this->queryActive = false;
    }

    /** @return array<string, int|float|bool> */
    public function run(string $uri, string $database, int $delayMs, int $tickMs): array
    {
        if (!(bool) ini_get('swow.enable')) {
            throw new RuntimeException('Swow coroutines must be enabled in both comparison groups');
        }
        $manager = new Manager($uri, [], ['disableClientPersistence' => true]);
        $collection = 'blocking_' . bin2hex(random_bytes(12));
        $namespace = $database . '.' . $collection;
        $done = new Channel(2);
        $stopped = false;
        $ticks = 0;
        $valid = false;
        $error = null;
        $timer = null;
        $query = null;
        try {
            $bulk = new BulkWrite();
            $bulk->insert(['_id' => 0]);
            $manager->executeBulkWrite($namespace, $bulk);
            $manager->executeCommand('admin', new Command(['ping' => 1]))->toArray();
            $filter = new Query(['_id' => 0, '$where' => sprintf(
                'function () { sleep(%d); return true; }', $delayMs
            )], ['limit' => 1]);
            addSubscriber($this);
            $timer = Coroutine::run(function () use (&$stopped, &$ticks, $done, $tickMs): void {
                $last = hrtime(true);
                while (!$stopped) {
                    usleep($tickMs * 1000);
                    $now = hrtime(true);
                    $this->maximumTickGapMs = max($this->maximumTickGapMs, ($now - $last) / 1e6);
                    $last = $now;
                    $ticks++;
                    // Count only while the real find command is pending,
                    // excluding setup, DNS and the post-query observation.
                    if ($this->queryActive) {
                        $this->ticksDuringQuery++;
                    }
                }
                $done->push(true);
            });
            // Establish that the observer is already running before MongoDB.
            usleep($tickMs * 3000);
            $ticksBeforeQuery = $ticks;
            $query = Coroutine::run(static function () use ($manager, $namespace, $filter, &$valid, &$error, $done): void {
                try {
                    $rows = $manager->executeQuery($namespace, $filter)->toArray();
                    $valid = count($rows) === 1 && $rows[0]->_id === 0;
                } catch (Throwable $exception) {
                    $error = $exception;
                } finally {
                    $done->push(true);
                }
            });
            $done->pop();
            // With no hook the overdue timer can resume only after find has
            // returned. Let it record that gap before stopping the observer.
            usleep($tickMs * 2000);
            $stopped = true;
            $done->pop();
            if ($error !== null) {
                throw $error;
            }
            if (!$valid || $this->queryEvents !== 1) {
                throw new RuntimeException('The probe did not complete exactly one valid find');
            }
            return [
                'coroutines_enabled' => true,
                'mongodb_hook' => Extension::isBuiltWith('mongodb'),
                'server_delay_ms' => $delayMs,
                'tick_interval_ms' => $tickMs,
                'ticks_before_query' => $ticksBeforeQuery,
                'query_ms' => $this->queryMs,
                'ticks_during_query' => $this->ticksDuringQuery,
                'maximum_tick_gap_ms' => $this->maximumTickGapMs,
                'result_valid' => $valid,
            ];
        } finally {
            if ($timer !== null && $timer->isAlive()) {
                $timer->kill();
            }
            if ($query !== null && $query->isAlive()) {
                $query->kill();
            }
            removeSubscriber($this);
            $manager->executeCommand($database, new Command(['drop' => $collection]))->toArray();
        }
    }
}
