--TEST--
swow_thread: wait timeout
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

$thread = new Swow\Thread(static function (): string {
    usleep(300000);
    return 'done';
});
$thread->start();

try {
    $thread->wait(10);
    echo "no timeout\n";
} catch (Swow\ThreadException $e) {
    echo "timeout\n";
}

var_dump($thread->wait());
var_dump($thread->getExitStatus());
?>
--EXPECT--
timeout
string(4) "done"
int(0)
