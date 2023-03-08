--TEST--
swow_pgsql: test query
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_env_not_true('TEST_SWOW_POSTGRESQL');
?>
--FILE--
<?php
use Swow\Coroutine;

use function Swow\Sync\waitAll;

require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/PDOUtil.inc';

PDOUtil::init();

$pdo = PDOUtil::create();

$stmt = $pdo->prepare('INSERT INTO test_swow_pgsql_users (name, age) values (?, ?)');

for ($i = 0; $i < 3; $i++) {
    $stmt->bindValue(1, base64_encode(random_bytes(8)));
    $stmt->bindValue(2, random_int(18, 35));
    $stmt->execute();
}

Coroutine::run(static function (): void {
    $pdo = PDOUtil::create();
    $statement = $pdo->query('select * from test_swow_pgsql_users where id = 1');
    var_dump($statement->fetchAll(PDO::FETCH_ASSOC));
});

var_dump('wait');

Coroutine::run(static function (): void {
    $pdo = PDOUtil::create();
    $statement = $pdo->query('select * from test_swow_pgsql_users where id = 2');
    var_dump($statement->fetchAll(PDO::FETCH_ASSOC));
});

waitAll();

echo "Done\n";
?>
--EXPECTF--
string(4) "wait"
array(1) {
  [0]=>
  array(3) {
    ["id"]=>
    int(%d)
    ["name"]=>
    string(%d) "%s"
    ["age"]=>
    int(%d)
  }
}
array(1) {
  [0]=>
  array(3) {
    ["id"]=>
    int(%d)
    ["name"]=>
    string(%d) "%s"
    ["age"]=>
    int(%d)
  }
}
Done
