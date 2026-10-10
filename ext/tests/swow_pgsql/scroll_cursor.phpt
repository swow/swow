--TEST--
swow_pgsql: scrollable cursor positioning, reexecution and cleanup
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

$pdo = PDOUtil::create();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$statement = null;
try {
    Assert::true($pdo->beginTransaction());
    $statement = $pdo->prepare('SELECT generate_series(CAST(:first AS integer), CAST(:last AS integer)) AS value', [
        PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL,
    ]);
    Assert::true($statement->execute(['first' => 1, 'last' => 3]));
    Assert::same($statement->columnCount(), 1);
    Assert::same($statement->getColumnMeta(0)['name'], 'value');
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_FIRST), ['value' => 1]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_LAST), ['value' => 3]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_PRIOR), ['value' => 2]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_ABS, 1), ['value' => 1]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_REL, 1), ['value' => 2]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_NEXT), ['value' => 3]);
    Assert::false($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_NEXT));
    Assert::true($statement->closeCursor());
    Assert::true($statement->execute(['first' => 4, 'last' => 5]));
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_FIRST), ['value' => 4]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_LAST), ['value' => 5]);
    Assert::true($statement->closeCursor());
    unset($statement);
    Assert::true($pdo->commit());
    Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
} finally {
    unset($statement);
    if ($pdo->inTransaction()) {
        $pdo->rollBack();
    }
}

echo "Done\n";
?>
--EXPECT--
Done
