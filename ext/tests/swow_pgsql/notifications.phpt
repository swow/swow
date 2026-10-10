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
    $listener->exec('UNLISTEN "' . $channel . '"');
}

echo "Done\n";
?>
--EXPECT--
Done
