--TEST--
swow_pgsql: parameter values and repeated execution in native and emulated prepares
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
    $pdo->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, false);
    $statement = $pdo->prepare('SELECT CAST(:flag AS boolean) AS flag,
        CAST(:number AS integer) AS number, CAST(:text AS text) AS text,
        CAST(:nullable AS text) AS nullable');
    $number = 7;
    $statement->bindParam(':number', $number, PDO::PARAM_INT);
    $statement->bindValue(':text', "O'Reilly ? :name", PDO::PARAM_STR);
    $statement->bindValue(':nullable', null, PDO::PARAM_NULL);
    foreach ([true, false] as $flag) {
        $statement->bindValue(':flag', $flag, PDO::PARAM_BOOL);
        $expectedNumber = $number;
        Assert::true($statement->execute());
        Assert::same($statement->fetch(PDO::FETCH_ASSOC), [
            'flag' => $flag,
            'number' => $expectedNumber,
            'text' => "O'Reilly ? :name",
            'nullable' => null,
        ]);
        Assert::false($statement->fetch());
        Assert::true($statement->closeCursor());
        $number++;
    }
    $statement = $pdo->prepare('SELECT CAST(? AS text) AS value');
    $statement->execute(['positional']);
    Assert::same($statement->fetchColumn(), 'positional');
    Assert::same($statement->columnCount(), 1);
    $metadata = $statement->getColumnMeta(0);
    Assert::same($metadata['name'], 'value');
    Assert::same($metadata['native_type'], 'text');
    Assert::same($pdo->query('SELECT ' . $pdo->quote("quote ' and \\ slash"))->fetchColumn(), "quote ' and \\ slash");
    unset($statement, $pdo);
}

echo "Done\n";
?>
--EXPECT--
Done
