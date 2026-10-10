--TEST--
swow_pgsql: quoted placeholders and transaction recovery after SQL errors
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
    $statement = $pdo->prepare(<<<'SQL'
        SELECT ':ignored ?' AS literal, $$:dollar ?$$ AS dollar,
            $tag$:tagged ?$tag$ AS tagged, CAST(:value AS integer) AS value
            /* :block ? */ -- :line ?
        SQL);
    $statement->execute(['value' => 42]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC), [
        'literal' => ':ignored ?',
        'dollar' => ':dollar ?',
        'tagged' => ':tagged ?',
        'value' => 42,
    ]);
    $statement->closeCursor();
    Assert::false($pdo->inTransaction());
    Assert::true($pdo->beginTransaction());
    Assert::true($pdo->inTransaction());
    Assert::throws(static function () use ($pdo): void {
        $pdo->query('SELECT 1 / 0');
    }, PDOException::class, expectMessage: '/22012/');
    Assert::same($pdo->errorCode(), '22012');
    Assert::same($pdo->errorInfo()[0], '22012');
    Assert::true($pdo->rollBack());
    Assert::false($pdo->inTransaction());
    Assert::same($pdo->query('SELECT 21 * 2')->fetchColumn(), 42);
    Assert::true($pdo->beginTransaction());
    Assert::true($pdo->commit());
    Assert::false($pdo->inTransaction());
    unset($statement, $pdo);
}

echo "Done\n";
?>
--EXPECT--
Done
