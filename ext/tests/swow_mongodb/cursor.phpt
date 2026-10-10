--TEST--
swow_mongodb: concurrent getMore and early cursor destruction preserve results
--SKIPIF--
<?php require __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
declare(strict_types=1);

use MongoDB\Driver\Query;
use Swow\Coroutine;

use function Swow\Sync\waitAll;

require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/MongoDBUtil.inc';

$managers = [MongoDBUtil::manager(), MongoDBUtil::manager()];
$collection = MongoDBUtil::collection();
MongoDBUtil::seed($managers[0], $collection, 12);
$results = [];
$coroutines = [];
try {
    foreach ($managers as $index => $manager) {
        $coroutines[] = Coroutine::run(static function () use ($manager, $collection, $index, &$results): void {
            $query = new Query([], ['batchSize' => 1, 'sort' => ['_id' => 1]]);
            $cursor = $manager->executeQuery(MongoDBUtil::database() . '.' . $collection, $query);
            $ids = [];
            foreach ($cursor as $document) {
                $ids[] = $document->_id;
                usleep(1000);
            }
            $results[$index] = $ids === range(0, 11);
            $cursor = $manager->executeQuery(MongoDBUtil::database() . '.' . $collection, $query);
            unset($cursor);
        });
    }
    waitAll();
    ksort($results);
    var_dump($results);
} finally {
    MongoDBUtil::drop($managers[0], $collection);
}
?>
--EXPECT--
array(2) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
}
