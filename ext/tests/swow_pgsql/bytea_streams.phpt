--TEST--
swow_pgsql: bytea stream parameters, binary results and resource ownership
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
    foreach (['', "a\0b\xff\\'\n"] as $body) {
        $input = tmpfile();
        $path = stream_get_meta_data($input)['uri'];
        fwrite($input, 'prefix' . $body);
        fseek($input, strlen('prefix'));
        $statement = $pdo->prepare('SELECT CAST(:body AS bytea) AS body, CAST(:label AS text) AS label');
        $statement->bindValue(':body', $input, PDO::PARAM_LOB);
        $statement->bindValue(':label', 'stream', PDO::PARAM_STR);
        Assert::true($statement->execute());
        $row = $statement->fetch(PDO::FETCH_ASSOC);
        Assert::same($row['label'], 'stream');
        Assert::true(is_resource($row['body']));
        Assert::same($statement->getColumnMeta(0)['native_type'], 'bytea');
        Assert::true($statement->closeCursor());
        unset($statement, $input);
        // Returned bytea data outlives the statement; the consumed input has no remaining owner.
        Assert::same(stream_get_contents($row['body']), $body);
        fclose($row['body']);
        unset($row);
        clearstatcache(true, $path);
        Assert::false(file_exists($path));
        Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
    }
}
echo "Done\n";
?>
--EXPECT--
Done
