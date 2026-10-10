--TEST--
swow_pgsql: driver-specific scanner preserves dollar-quoted placeholders
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_env_not_true('TEST_SWOW_POSTGRESQL');
skip_if(!Swow\Extension::isBuiltWith('pgsql'), 'pgsql is not built in');
skip_if(PHP_VERSION_ID < 80400, 'driver-specific PDO scanners require PHP 8.4');
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
        SELECT $$:dollar ?$$ AS dollar, $tag$:tagged ?$tag$ AS tagged,
            CAST(:value AS integer) AS value
        SQL);
    $statement->execute(['value' => 42]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC), [
        'dollar' => ':dollar ?',
        'tagged' => ':tagged ?',
        'value' => 42,
    ]);
    $statement->closeCursor();
    $statement = $pdo->prepare(<<<'SQL'
        SELECT 'quote'' ? :ignored' AS "quoted""name ? :ignored",
            E'backslash \\ and quote \' ? :ignored' AS escaped,
            $long_tag$one $short$ ? :ignored$long_tag$ AS tagged,
            CAST(:value AS integer) AS value
            /* ? :ignored */ -- ? :ignored
        SQL);
    $statement->execute(['value' => 7]);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC), [
        'quoted"name ? :ignored' => "quote' ? :ignored",
        'escaped' => "backslash \\ and quote ' ? :ignored",
        'tagged' => 'one $short$ ? :ignored',
        'value' => 7,
    ]);
    $statement->closeCursor();
    $statement = $pdo->prepare(<<<'SQL'
        SELECT CAST(:document AS jsonb) ?? CAST(:key AS text) AS present
        SQL);
    $statement->execute(['document' => '{"present":true}', 'key' => 'present']);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC), ['present' => true]);
    $statement->closeCursor();
    $statement->execute(['document' => '{"present":true}', 'key' => 'missing']);
    Assert::same($statement->fetch(PDO::FETCH_ASSOC), ['present' => false]);
    $statement->closeCursor();
    $statement = $pdo->prepare('SELECT CAST(:value AS integer) + CAST(:value AS integer) AS repeated');
    $statement->execute(['value' => 21]);
    Assert::same($statement->fetchColumn(), 42);
    $statement->closeCursor();
}

echo "Done\n";
?>
--EXPECT--
Done
