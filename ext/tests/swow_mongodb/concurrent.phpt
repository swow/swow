--TEST--
swow_mongodb: independent native clients overlap while another coroutine runs
--SKIPIF--
<?php require __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
declare(strict_types=1);

use MongoDB\Driver\Command;
use MongoDB\Driver\Query;
use Swow\Coroutine;

use function MongoDB\Driver\Monitoring\addSubscriber;
use function MongoDB\Driver\Monitoring\removeSubscriber;
use function Swow\Sync\waitAll;

require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/MongoDBUtil.inc';

$managers = [MongoDBUtil::manager(false), MongoDBUtil::manager(false)];
$collection = MongoDBUtil::collection();
MongoDBUtil::seed($managers[0], $collection, 1);
// Warm both transports so DNS alone cannot make this concurrency check pass.
foreach ($managers as $manager) {
    $manager->executeCommand('admin', new Command(['ping' => 1]))->toArray();
}
$tracker = new MongoDBCommandTracker();
$completed = 0;
$ticks = 0;
$coroutines = [];
addSubscriber($tracker);
try {
    foreach ($managers as $manager) {
        $coroutines[] = Coroutine::run(static function () use ($manager, $collection, &$completed): void {
            // Real server-side work keeps the reply pending. No fake MongoDB
            // server or client-side sleep stands in for database I/O.
            $query = new Query(['$where' => 'function () { sleep(100); return true; }'], ['limit' => 1]);
            $rows = $manager->executeQuery(MongoDBUtil::database() . '.' . $collection, $query)->toArray();
            if (count($rows) !== 1 || $rows[0]->_id !== 0) {
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
    var_dump($completed === 2, $tracker->maximum === 2, $tracker->active === 0, $ticks > 0);
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
