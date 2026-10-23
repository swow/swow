--TEST--
swow_thread: child RSHUTDOWN joins its own children
--SKIPIF--
<?php
if (!PHP_ZTS) {
    die('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$thread = new Swow\Thread(static function (): void {
    $grandchild = new Swow\Thread(static function (): void {
        usleep(200000);
        echo "grandchild done\n";
    });
    $grandchild->start();
});
$thread->start();
$thread->wait();

echo "parent done\n";
?>
--EXPECT--
grandchild done
parent done
