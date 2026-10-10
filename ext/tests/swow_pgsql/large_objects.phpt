--TEST--
swow_pgsql: large object stream operations and transaction cleanup
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
$stream = null;
$deprecations = [];
// Legacy PDO methods remain callable in PHP 8.5 and must report their deprecation.
set_error_handler(static function (int $severity, string $message) use (&$deprecations): bool {
    if ($severity !== E_DEPRECATED || !preg_match('/^Method PDO::(pgsqlLOBCreate|pgsqlLOBOpen|pgsqlLOBUnlink)\(\) is deprecated since 8\.5,/', $message, $matches)) {
        return false;
    }
    $deprecations[] = $matches[1];
    return true;
});
try {
    Assert::true($pdo->beginTransaction());
    $oid = $pdo->pgsqlLOBCreate();
    Assert::true(ctype_digit($oid));
    $stream = $pdo->pgsqlLOBOpen($oid, 'w+b');
    Assert::true(is_resource($stream));
    Assert::same(fwrite($stream, "first\0last"), 10);
    Assert::true(fflush($stream));
    Assert::same(ftell($stream), 10);
    Assert::true(rewind($stream));
    Assert::same(fread($stream, 5), 'first');
    Assert::same(fseek($stream, -4, SEEK_END), 0);
    Assert::same(fread($stream, 4), 'last');
    Assert::true(fclose($stream));
    $stream = $pdo->pgsqlLOBOpen($oid, 'rb');
    Assert::same(stream_get_contents($stream), "first\0last");
    Assert::true(fclose($stream));
    $stream = null;
    Assert::true($pdo->pgsqlLOBUnlink($oid));
} finally {
    restore_error_handler();
    if (is_resource($stream)) {
        fclose($stream);
    }
    if ($pdo->inTransaction()) {
        $pdo->rollBack();
    }
}

Assert::same(array_count_values($deprecations), PHP_VERSION_ID >= 80500 ? [
    'pgsqlLOBCreate' => 1,
    'pgsqlLOBOpen' => 2,
    'pgsqlLOBUnlink' => 1,
] : []);

echo "Done\n";
?>
--EXPECT--
Done
