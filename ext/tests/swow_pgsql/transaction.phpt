--TEST--
swow_pgsql: test transaction
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_env_not_true('TEST_SWOW_POSTGRESQL');
skip_if(!Swow\Extension::isBuiltWith('pgsql'), 'pgsql is not built in');
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
$stmt->bindValue(1, base64_encode(random_bytes(8)));
$stmt->bindValue(2, random_int(18, 35));
$stmt->execute();

Coroutine::run(static function (): void {
    $pdo = PDOUtil::create();
    try {
        $pdo->beginTransaction();

        $pdo->exec('DROP TABLE IF EXISTS test_swow_pgsql_users');
        throw new Exception('interrupt!!!');
        $pdo->commit();
    } catch (\Exception $e) {
        $pdo->rollBack();
        var_dump('rollback');
    }
});

var_dump('wait1');
waitAll();

var_dump(PDOUtil::tableExists($pdo, 'test_swow_pgsql_users'));

Coroutine::run(static function (): void {
    $pdo = PDOUtil::create();
    try {
        $pdo->beginTransaction();

        $pdo->exec('DROP TABLE IF EXISTS test_swow_pgsql_users');
        $pdo->commit();
    } catch (\Exception $e) {
        $pdo->rollBack();
        var_dump($e->getMessage());
    }
});

var_dump('wait2');
waitAll();
var_dump(PDOUtil::tableExists($pdo, 'test_swow_pgsql_users'));

echo "Done\n";
?>
--EXPECTF--
string(5) "wait1"
string(8) "rollback"
bool(true)
string(5) "wait2"
bool(false)
Done
