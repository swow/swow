--TEST--
swow_pgsql: connection attributes, statement modes and error recovery
--EXTENSIONS--
pdo_pgsql
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
$disablePrepares = PHP_VERSION_ID >= 80400 ? Pdo\Pgsql::ATTR_DISABLE_PREPARES : PDO::PGSQL_ATTR_DISABLE_PREPARES;
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
Assert::same($pdo->getAttribute(PDO::ATTR_DRIVER_NAME), 'pgsql');
Assert::same($pdo->getAttribute(PDO::ATTR_SERVER_VERSION), $pdo->query('SHOW server_version')->fetchColumn());
Assert::true(is_string($pdo->getAttribute(PDO::ATTR_CLIENT_VERSION)));
Assert::contains($pdo->getAttribute(PDO::ATTR_CONNECTION_STATUS), 'Connection OK');
$serverInfo = $pdo->getAttribute(PDO::ATTR_SERVER_INFO);
Assert::contains($serverInfo, 'PID: ' . $pdo->query('SELECT pg_backend_pid()')->fetchColumn());
Assert::contains($serverInfo, 'Client Encoding: ' . $pdo->query('SHOW client_encoding')->fetchColumn());

foreach ([[false, false], [false, true], [true, false]] as [$emulate, $disable]) {
    Assert::true($pdo->setAttribute(PDO::ATTR_EMULATE_PREPARES, $emulate));
    Assert::true($pdo->setAttribute($disablePrepares, $disable));
    Assert::same($pdo->getAttribute(PDO::ATTR_EMULATE_PREPARES), $emulate);
    Assert::same($pdo->getAttribute($disablePrepares), $disable);
    $statement = $pdo->prepare('SELECT CAST(:value AS text)');
    foreach (["a'b\\c", 'second'] as $value) {
        Assert::true($statement->execute(['value' => $value]));
        Assert::same($statement->fetchColumn(), $value);
        Assert::true($statement->closeCursor());
    }
}
Assert::true($pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT));
Assert::false($pdo->query('SELECT 1 / 0'));
Assert::same($pdo->errorCode(), '22012');
Assert::same($pdo->errorInfo()[0], '22012');
Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
Assert::true($pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION));
Assert::throws(static fn() => $pdo->exec('SELECT 1 / 0'), PDOException::class);
Assert::same($pdo->query('SELECT 43')->fetchColumn(), 43);
echo "Done\n";
?>
--EXPECT--
Done
