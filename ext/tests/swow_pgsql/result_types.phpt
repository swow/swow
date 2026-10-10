--TEST--
swow_pgsql: scalar result types, column metadata and stringify fetches
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
$pdo->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, false);
$statement = $pdo->query(<<<'SQL'
    SELECT true AS flag, -7::smallint AS small, 42::integer AS number,
        123456789::bigint AS large, 23::oid AS object_id,
        0.5::real AS single, -0.25::double precision AS double,
        'plain'::text AS text, 'bounded'::varchar(12) AS variable,
        DATE '2020-02-03' AS date, TIMESTAMP '2020-02-03 04:05:06' AS timestamp,
        '12.50'::numeric AS decimal, NULL::text AS nullable,
        'Infinity'::double precision AS positive_infinity,
        '-Infinity'::real AS negative_infinity, 'NaN'::double precision AS nan
    SQL);
$types = ['bool', 'int2', 'int4', 'int8', 'oid', 'float4', 'float8', 'text', 'varchar', 'date', 'timestamp', 'numeric', 'text', 'float8', 'float4', 'float8'];
$names = ['flag', 'small', 'number', 'large', 'object_id', 'single', 'double', 'text', 'variable', 'date', 'timestamp', 'decimal', 'nullable', 'positive_infinity', 'negative_infinity', 'nan'];
Assert::same($statement->columnCount(), count($names));
foreach ($types as $index => $type) {
    $metadata = $statement->getColumnMeta($index);
    Assert::same($metadata['name'], $names[$index]);
    Assert::same($metadata['native_type'], $type);
}
$row = $statement->fetch(PDO::FETCH_ASSOC);
Assert::same(array_slice($row, 0, 13), [
    'flag' => true,
    'small' => -7,
    'number' => 42,
    'large' => 123456789,
    'object_id' => 23,
    'single' => 0.5,
    'double' => -0.25,
    'text' => 'plain',
    'variable' => 'bounded',
    'date' => '2020-02-03',
    'timestamp' => '2020-02-03 04:05:06',
    'decimal' => '12.50',
    'nullable' => null,
]);
Assert::true(is_infinite($row['positive_infinity']));
Assert::greaterThan($row['positive_infinity'], 0);
Assert::true(is_infinite($row['negative_infinity']));
Assert::lessThan($row['negative_infinity'], 0);
Assert::true(is_nan($row['nan']));
Assert::false($statement->fetch());
Assert::true($statement->closeCursor());

Assert::true($pdo->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, true));
Assert::same($pdo->query('SELECT false AS flag, -7::smallint AS small, 0.5::real AS fraction, NULL::text AS nullable')->fetch(PDO::FETCH_ASSOC), [
    'flag' => '0',
    'small' => '-7',
    'fraction' => '0.5',
    'nullable' => null,
]);
echo "Done\n";
?>
--EXPECT--
Done
