--TEST--
swow_pgsql: test hook pgsql
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
use Swow\Coroutine;

use function Swow\Sync\waitAll;

require __DIR__ . '/../include/bootstrap.php';

$host = TEST_POSTGRES_HOST;
$port = TEST_POSTGRES_PORT;
$user = TEST_POSTGRES_USER;
$password = TEST_POSTGRES_PASSWORD;
$dbname = TEST_POSTGRES_DBNAME;

$dsn = "pgsql:host={$host};port={$port};dbname={$dbname}";

Coroutine::run(static function () use ($dsn, $user, $password): void {
    $pdo = new PDO($dsn, $user, $password);
    $statement = $pdo->prepare('SELECT * FROM pg_catalog.pg_tables limit 1');
    $statement->execute();
    var_dump($statement->fetchAll(PDO::FETCH_COLUMN));
});

var_dump('wait');

Coroutine::run(static function () use ($dsn, $user, $password): void {
  $pdo = new PDO($dsn, $user, $password);
  $statement = $pdo->prepare('SELECT * FROM pg_catalog.pg_tables limit 1');
  $statement->execute();
  var_dump($statement->fetchAll(PDO::FETCH_COLUMN));
});

waitAll();

echo "Done\n";
?>
--EXPECT--
string(4) "wait"
array(1) {
  [0]=>
  string(6) "public"
}
array(1) {
  [0]=>
  string(6) "public"
}
Done
