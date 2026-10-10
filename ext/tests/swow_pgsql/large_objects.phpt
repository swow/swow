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
    if (is_resource($stream)) {
        fclose($stream);
    }
    if ($pdo->inTransaction()) {
        $pdo->rollBack();
    }
}

echo "Done\n";
?>
--EXPECT--
Done
