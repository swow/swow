--TEST--
swow_pgsql: notice callback ownership, replacement and removal
--EXTENSIONS--
pdo_pgsql
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_env_not_true('TEST_SWOW_POSTGRESQL');
skip_if(!Swow\Extension::isBuiltWith('pgsql'), 'pgsql is not built in');
skip_if(PHP_VERSION_ID < 80400, 'notice callbacks require PHP 8.4');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
require __DIR__ . '/PDOUtil.inc';

$pdo = PDOUtil::create(Pdo\Pgsql::class);
$notices = [];
$callback = static function (string $notice) use (&$notices): void {
    $notices[] = $notice;
};
$reference = WeakReference::create($callback);
$pdo->setNoticeCallback($callback);
unset($callback);
gc_collect_cycles();
Assert::notNull($reference->get());
$pdo->exec("DO \$\$ BEGIN RAISE NOTICE 'swow-first'; END \$\$");
Assert::same(count($notices), 1);
Assert::contains($notices[0], 'swow-first');
$replacement = [];
$pdo->setNoticeCallback(static function (string $notice) use (&$replacement): void {
    $replacement[] = $notice;
});
gc_collect_cycles();
Assert::null($reference->get());
$pdo->exec("DO \$\$ BEGIN RAISE NOTICE 'swow-second'; END \$\$");
Assert::same(count($notices), 1);
Assert::same(count($replacement), 1);
Assert::contains($replacement[0], 'swow-second');
$pdo->setNoticeCallback(null);
$pdo->exec("DO \$\$ BEGIN RAISE NOTICE 'swow-third'; END \$\$");
Assert::same(count($replacement), 1);
Assert::same($pdo->query('SELECT 42')->fetchColumn(), 42);
echo "Done\n";
?>
--EXPECT--
Done
