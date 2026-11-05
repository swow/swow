--TEST--
swow_pgsql: a re-executed statement resizes its column description
--CONFLICTS--
pgsql_test_swow_pgsql_users
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_env_not_true('TEST_SWOW_POSTGRESQL');
skip_if(!Swow\Extension::isBuiltWith('pgsql'), 'pgsql is not built in');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/PDOUtil.inc';

/*
 * The statement keeps the column description of its first result. A second
 * execute can return more columns, and the fetch then reads past both the
 * internal column array and the one PDO uses to name the columns.
 *
 * The query must be sent again on every execute, otherwise the server rejects
 * the changed result type of a cached plan, so emulated prepares are required.
 */

$pdo = PDOUtil::create();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->setAttribute(PDO::ATTR_EMULATE_PREPARES, true);

$pdo->exec('CREATE TEMPORARY TABLE swow_pgsql_growing_columns (a integer)');
$pdo->exec('INSERT INTO swow_pgsql_growing_columns VALUES (1)');

$stmt = $pdo->prepare('SELECT *, ? AS extra FROM swow_pgsql_growing_columns');
$stmt->execute([7]);
Assert::same($stmt->columnCount(), 2);
Assert::eq([1, '7'], $stmt->fetch(PDO::FETCH_NUM));

// the second result has one more column than the description of the first
$pdo->exec('ALTER TABLE swow_pgsql_growing_columns ADD COLUMN b text');
$pdo->exec("UPDATE swow_pgsql_growing_columns SET b = 'x'");

$stmt->execute([7]);
Assert::same($stmt->columnCount(), 3);
Assert::eq([1, 'x', '7'], $stmt->fetch(PDO::FETCH_NUM));

echo "Done\n";

?>
--EXPECT--
Done
