--TEST--
swow_thread: RSHUTDOWN waits for children
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$thread = new Swow\Thread(static function (): void {
    usleep(200000);
    echo "child done\n";
});
$thread->start();
unset($thread);

echo "parent end\n";
?>
--EXPECT--
parent end
child done
