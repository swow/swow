<?php

declare(strict_types=1);

use MongoDB\Driver\BulkWrite;
use MongoDB\Driver\Command;
use MongoDB\Driver\Manager;
use MongoDB\Driver\Monitoring\CommandFailedEvent;
use MongoDB\Driver\Monitoring\CommandStartedEvent;
use MongoDB\Driver\Monitoring\CommandSubscriber;
use MongoDB\Driver\Monitoring\CommandSucceededEvent;

final class MongoDBUtil
{
    public static function uri(): string
    {
        return getenv('MONGODB_URI') ?: 'mongodb://127.0.0.1/';
    }

    public static function manager(bool $persistent = true): Manager
    {
        return new Manager(self::uri(), [], ['disableClientPersistence' => !$persistent]);
    }

    public static function database(): string
    {
        return getenv('MONGODB_DATABASE') ?: 'swow_test';
    }

    public static function collection(): string
    {
        return 'mongodb_hook_' . bin2hex(random_bytes(8));
    }

    public static function seed(Manager $manager, string $collection, int $count): void
    {
        $bulk = new BulkWrite();
        for ($id = 0; $id < $count; $id++) {
            $bulk->insert(['_id' => $id, 'value' => $id]);
        }
        $manager->executeBulkWrite(self::database() . '.' . $collection, $bulk);
    }

    public static function drop(Manager $manager, string $collection): void
    {
        $manager->executeCommand(self::database(), new Command(['drop' => $collection]))->toArray();
    }
}

final class MongoDBCommandTracker implements CommandSubscriber
{
    public int $active = 0;
    public int $maximum = 0;

    private function tracks(string $name): bool
    {
        return in_array($name, ['find', 'getMore'], true);
    }

    public function commandStarted(CommandStartedEvent $event): void
    {
        if ($this->tracks($event->getCommandName())) {
            $this->active++;
            $this->maximum = max($this->maximum, $this->active);
        }
    }

    public function commandSucceeded(CommandSucceededEvent $event): void
    {
        if ($this->tracks($event->getCommandName())) {
            $this->active--;
        }
    }

    public function commandFailed(CommandFailedEvent $event): void
    {
        if ($this->tracks($event->getCommandName())) {
            $this->active--;
        }
    }
}
