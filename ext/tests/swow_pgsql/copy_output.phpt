--TEST--
swow_pgsql: COPY output formatting, file export and connection reuse
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
$filename = tempnam(sys_get_temp_dir(), 'swow-copy-');
$deprecations = [];
set_error_handler(static function (int $severity, string $message) use (&$deprecations): bool {
    if ($severity !== E_DEPRECATED || !preg_match('/^Method PDO::(pgsqlCopyToArray|pgsqlCopyToFile)\(\) is deprecated since 8\.5,/', $message, $matches)) {
        return false;
    }
    $deprecations[] = $matches[1];
    return true;
});
try {
    // A SELECT export needs no fixture table or schema changes.
    $source = "(SELECT * FROM (VALUES (1, 'alpha'::text), (2, NULL::text)) AS rows(id, value) ORDER BY id)";
    Assert::same($pdo->pgsqlCopyToArray($source), ["1\talpha\n", "2\t\\N\n"]);
    Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
    Assert::same($pdo->pgsqlCopyToArray($source, '|', 'NULL'), ["1|alpha\n", "2|NULL\n"]);
    Assert::true($pdo->pgsqlCopyToFile($source, $filename, '|', 'NULL'));
    Assert::same(file_get_contents($filename), "1|alpha\n2|NULL\n");
    Assert::same($pdo->query('SELECT 43')->fetchColumn(), 43);
    Assert::same($pdo->pgsqlCopyToArray('(SELECT 1 WHERE false)'), []);
    Assert::true($pdo->pgsqlCopyToFile('(SELECT 1 WHERE false)', $filename));
    Assert::same(file_get_contents($filename), '');
    Assert::throws(static fn() => $pdo->pgsqlCopyToArray('(SELECT 1 / 0)'), PDOException::class);
    Assert::same($pdo->query('SELECT 44')->fetchColumn(), 44);
    Assert::throws(static fn() => $pdo->pgsqlCopyToFile('(SELECT 1 / 0)', $filename), PDOException::class);
    Assert::same($pdo->query('SELECT 45')->fetchColumn(), 45);
} finally {
    restore_error_handler();
    unlink($filename);
}
Assert::same(array_count_values($deprecations), PHP_VERSION_ID >= 80500 ? [
    'pgsqlCopyToArray' => 4,
    'pgsqlCopyToFile' => 3,
] : []);
echo "Done\n";
?>
--EXPECT--
Done
