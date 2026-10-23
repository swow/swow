--TEST--
swow_thread: detach
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
    usleep(100000);
});
$thread->start();
$thread->detach();
var_dump($thread->isDetached());
try {
    $thread->wait();
} catch (Swow\ThreadException $e) {
    echo "detached\n";
}
usleep(200000);
echo "done\n";
?>
--EXPECT--
bool(true)
detached
done
