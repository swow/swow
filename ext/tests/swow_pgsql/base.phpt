--TEST--
swow_pgsql: test hook pgsql
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_env_not_true('TEST_SWOW_POSTGRESQL');
skip_if_extension_not_exist('pdo_pgsql');
?>
--FILE--
<?php
use Swow\Coroutine;

use function Swow\Sync\waitAll;

require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/PDOUtil.inc';

PDOUtil::init();

Coroutine::run(static function (): void {
    $pdo = PDOUtil::create();
    $statement = $pdo->prepare('SELECT * FROM pg_catalog.pg_tables limit 1');
    $statement->execute();
    var_dump($statement->fetchAll(PDO::FETCH_COLUMN));
});

var_dump('wait');

Coroutine::run(static function (): void {
    $pdo = PDOUtil::create();
    $statement = $pdo->prepare('SELECT * FROM pg_catalog.pg_tables limit 1');
    $statement->execute();
    var_dump($statement->fetchAll(PDO::FETCH_COLUMN));
});

waitAll();

echo "Done\n";
?>
--EXPECTF--
string(4) "wait"
array(1) {
  [0]=>
  string(%d) "%s"
}
array(1) {
  [0]=>
  string(%d) "%s"
}
Done
