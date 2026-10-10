--TEST--
swow_pgsql: LISTEN notifications preserve channel, sender and payload
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

$listener = PDOUtil::create();
$sender = PDOUtil::create();
$channel = 'swow_notify_' . getRandomBytes(12);
$listener->exec('LISTEN "' . $channel . '"');
$deprecations = [];
// Assert the PHP 8.5 legacy-method notices without hiding unrelated diagnostics.
set_error_handler(static function (int $severity, string $message) use (&$deprecations): bool {
    if ($severity !== E_DEPRECATED || !preg_match('/^Method PDO::(pgsqlGetNotify|pgsqlGetPid)\(\) is deprecated since 8\.5,/', $message, $matches)) {
        return false;
    }
    $deprecations[] = $matches[1];
    return true;
});
try {
    Assert::false($listener->pgsqlGetNotify(PDO::FETCH_ASSOC));
    $statement = $sender->prepare('SELECT pg_notify(:channel, :payload)');
    foreach ([PDO::FETCH_ASSOC, PDO::FETCH_NUM, PDO::FETCH_BOTH] as $mode) {
        $statement->execute(['channel' => $channel, 'payload' => 'message']);
        $statement->closeCursor();
        $notification = $listener->pgsqlGetNotify($mode, 1000);
        if ($mode === PDO::FETCH_ASSOC || $mode === PDO::FETCH_BOTH) {
            Assert::same($notification['message'], $channel);
            Assert::same($notification['pid'], $sender->pgsqlGetPid());
            Assert::same($notification['payload'], 'message');
        }
        if ($mode === PDO::FETCH_NUM || $mode === PDO::FETCH_BOTH) {
            Assert::same($notification[0], $channel);
            Assert::same($notification[1], $sender->pgsqlGetPid());
            Assert::same($notification[2], 'message');
        }
    }
    Assert::false($listener->pgsqlGetNotify(PDO::FETCH_ASSOC));
} finally {
    restore_error_handler();
    $listener->exec('UNLISTEN "' . $channel . '"');
}

Assert::same(array_count_values($deprecations), PHP_VERSION_ID >= 80500 ? [
    'pgsqlGetNotify' => 5,
    'pgsqlGetPid' => 4,
] : []);

echo "Done\n";
?>
--EXPECT--
Done
