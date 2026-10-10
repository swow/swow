--TEST--
swow_mongodb: Managers sharing a persistent client serialize native commands
--SKIPIF--
<?php require __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
declare(strict_types=1);

use MongoDB\Driver\Query;
use Swow\Coroutine;

use function MongoDB\Driver\Monitoring\addSubscriber;
use function MongoDB\Driver\Monitoring\removeSubscriber;
use function Swow\Sync\waitAll;

require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/MongoDBUtil.inc';

$managers = [MongoDBUtil::manager(), MongoDBUtil::manager()];
$collection = MongoDBUtil::collection();
MongoDBUtil::seed($managers[0], $collection, 1);
$tracker = new MongoDBCommandTracker();
$completed = 0;
$ticks = 0;
$coroutines = [];
addSubscriber($tracker);
try {
    foreach ($managers as $manager) {
        $coroutines[] = Coroutine::run(static function () use ($manager, $collection, &$completed): void {
            $query = new Query(['$where' => 'function () { sleep(100); return true; }'], ['limit' => 1]);
            $rows = $manager->executeQuery(MongoDBUtil::database() . '.' . $collection, $query)->toArray();
            if (count($rows) !== 1) {
                throw new RuntimeException('Unexpected MongoDB query result');
            }
            $completed++;
        });
    }
    $coroutines[] = Coroutine::run(static function () use (&$completed, &$ticks, $tracker): void {
        while ($completed !== 2) {
            usleep(1000);
            if ($tracker->active > 0) {
                $ticks++;
            }
        }
    });
    waitAll();
    var_dump($completed === 2, $tracker->maximum === 1, $tracker->active === 0, $ticks > 0);
} finally {
    removeSubscriber($tracker);
    MongoDBUtil::drop($managers[0], $collection);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
