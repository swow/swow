--TEST--
swow_pgsql: text and binary quoting preserve values under PostgreSQL string modes
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
$stringMode = $pdo->query('SHOW standard_conforming_strings')->fetchColumn();
$warnings = $pdo->query('SHOW escape_string_warning')->fetchColumn();
$encoding = $pdo->query('SHOW client_encoding')->fetchColumn();
try {
    $pdo->exec("SET client_encoding = 'UTF8'");
    $pdo->exec('SET escape_string_warning = off');
    foreach (['on', 'off'] as $mode) {
        $pdo->exec('SET standard_conforming_strings = ' . $mode);
        foreach (['', "quote ' and \\ slash", "line\nbreak", 'café'] as $text) {
            $quoted = $pdo->quote($text, PDO::PARAM_STR);
            Assert::string($quoted);
            Assert::same($pdo->query('SELECT ' . $quoted . '::text')->fetchColumn(), $text);
        }
        foreach (['', "binary\0body'\\", implode('', array_map('chr', range(0, 255)))] as $data) {
            $quoted = $pdo->quote($data, PDO::PARAM_LOB);
            Assert::string($quoted);
            $stream = $pdo->query('SELECT ' . $quoted . '::bytea')->fetchColumn();
            try {
                Assert::true(is_resource($stream));
                Assert::same(stream_get_contents($stream), $data);
            } finally {
                if (is_resource($stream)) {
                    fclose($stream);
                }
            }
        }
    }
    // Invalid text encoding cannot be quoted as a PostgreSQL UTF-8 string; binary data uses PARAM_LOB.
    Assert::false($pdo->quote("invalid\xff", PDO::PARAM_STR));
    Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
} finally {
    $pdo->exec('SET standard_conforming_strings = ' . $stringMode);
    $pdo->exec('SET escape_string_warning = ' . $warnings);
    $pdo->exec('SET client_encoding = ' . $pdo->quote($encoding));
}
echo "Done\n";
?>
--EXPECT--
Done
