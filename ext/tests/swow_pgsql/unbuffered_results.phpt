--TEST--
swow_pgsql: single-row fetching, unfinished result cleanup and query errors
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

foreach ([false, true] as $emulate) {
    $pdo = PDOUtil::create();
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $pdo->setAttribute(PDO::ATTR_EMULATE_PREPARES, $emulate);
    Assert::true($pdo->setAttribute(PDO::ATTR_PREFETCH, 0));
    $statement = $pdo->prepare('SELECT generate_series(1, CAST(:last AS integer)) AS value');
    Assert::true($statement->execute(['last' => 3]));
    Assert::same($statement->columnCount(), 1);
    Assert::same($statement->getColumnMeta(0)['name'], 'value');
    Assert::same($statement->fetchColumn(), 1);
    Assert::same($statement->fetchColumn(), 2);
    Assert::same($statement->fetchColumn(), 3);
    Assert::false($statement->fetchColumn());
    Assert::true($statement->closeCursor());

    Assert::true($statement->execute(['last' => 4]));
    Assert::same($statement->fetchColumn(), 1);
    Assert::true($statement->closeCursor());
    Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
    Assert::true($statement->execute(['last' => 2]));
    Assert::same($statement->fetchAll(PDO::FETCH_COLUMN), [1, 2]);
    Assert::true($statement->closeCursor());

    $failed = $pdo->query('SELECT value / (4 - value) AS result FROM generate_series(1, 4) AS value');
    Assert::same($failed->fetchColumn(), 0);
    Assert::same($failed->fetchColumn(), 1);
    Assert::same($failed->fetchColumn(), 3);
    Assert::throws(static function () use ($failed): void {
        $failed->fetchColumn();
    }, PDOException::class, expectMessage: '/22012/');
    Assert::same($failed->errorCode(), '22012');
    Assert::true($failed->closeCursor());
    Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
    unset($failed, $statement, $pdo);
}

echo "Done\n";
?>
--EXPECT--
Done
