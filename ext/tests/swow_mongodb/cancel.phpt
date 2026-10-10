--TEST--
swow_mongodb: cancelling an owner and a waiter leaves the native client usable
--SKIPIF--
<?php require __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
declare(strict_types=1);

use MongoDB\Driver\Command;
use MongoDB\Driver\Query;
use Swow\Coroutine;

require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/MongoDBUtil.inc';

$manager = MongoDBUtil::manager();
$collection = MongoDBUtil::collection();
MongoDBUtil::seed($manager, $collection, 1);
try {
    $owner = Coroutine::run(static function () use ($manager, $collection): void {
        $query = new Query(['$where' => 'function () { sleep(200); return true; }'], ['limit' => 1]);
        $manager->executeQuery(MongoDBUtil::database() . '.' . $collection, $query)->toArray();
    });
    $waiter = Coroutine::run(static function () use ($manager): void {
        $manager->executeCommand('admin', new Command(['ping' => 1]))->toArray();
    });
    // The first coroutine waits for a real reply, and the second waits for
    // ownership of the same native client. Both paths must release their state.
    $waiter->kill();
    $owner->kill();
    var_dump(!$waiter->isAlive(), !$owner->isAlive());
    $rows = $manager->executeQuery(MongoDBUtil::database() . '.' . $collection, new Query([]))->toArray();
    var_dump(count($rows) === 1, $rows[0]->_id === 0);
} finally {
    MongoDBUtil::drop($manager, $collection);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
